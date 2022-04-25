#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <cfloat>
#include <atomic>
#include <mutex>
#include <condition_variable>


#ifndef MS_STDLIB_BUGS // Allow overriding the autodetection.
/* The Microsoft C and C++ runtime libraries that ship with Visual Studio, as
 * of 2017, have a bug that neither stdio, iostreams or wide iostreams can
 * handle Unicode input or output.  Windows needs some non-standard magic to
 * work around that.  This includes programs compiled with MinGW and Clang
 * for the win32 and win64 targets.
 */
#  if ( _MSC_VER || __MINGW32__ || __MSVCRT__ )
 /* This code is being compiled either on MS Visual C++, or MinGW, or
  * clang++ in compatibility mode for either, or is being linked to the
  * msvcrt.dll runtime.
  */
#    define MS_STDLIB_BUGS 1
#  else
#    define MS_STDLIB_BUGS 0
#  endif
#endif

#if MS_STDLIB_BUGS
#  include <io.h>
#  include <fcntl.h>
#endif


using namespace std;
void init_locale(void)
// Does magic so that wcout can work.
{
#if MS_STDLIB_BUGS
    // Windows needs a little non-standard magic.
    constexpr char cp_utf16le[] = ".1200"; // UTF-16 little-endian locale.
    setlocale(LC_ALL, cp_utf16le);
    _setmode(_fileno(stdout), _O_WTEXT);

    /* Repeat for _fileno(stdin), if needed. */
#else
    // The correct locale name may vary by OS, e.g., "en_US.utf8".
    constexpr char locale_name[] = "";
    setlocale(LC_ALL, locale_name);
    std::locale::global(std::locale(locale_name));
    std::wcin.imbue(std::locale())
        std::wcout.imbue(std::locale());

#endif
}


class Board;

const int kingPrice = 1000000000;

bool itsTimeToStop;//atomic too slow
bool evaluateMoves = false;
char depthW;
char onMoveW;


struct Piece {
    virtual wchar_t print() const = 0;/*
    {
        return L' ';
    }*/

    virtual char occupancy() const = 0;/*
    {
        return 0;
    }*/

    virtual double price(char row) const = 0;/*
    {
        return 0;
    }*/

    virtual Piece* clone() const = 0;/*
    {
        return new Piece(*this);
    }*/

    virtual char printBasic() const = 0;/*
    {
        return ' ';
    }*/

    virtual double bestPosition(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNotContinue = false) = 0;/*
    {
        return 0;
    }*/

    virtual void tryChangeAndUpdateIfBetter(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, double& bestValue, double& totalValues, uint_fast16_t& totalMoves, bool& doNotContinue, double valueSoFar, Piece* changeInto = nullptr,  double minPriceToTake = 0, double maxPriceToTake = DBL_MAX);

};

bool saveToVector = false;

//const Piece emptyField;

class Board {
    Piece* pieces[64];
public:

    Board(const Board& copy)
    {
        for (uint_fast8_t i = 0; i < 64; ++i) {
            if (copy.pieces[i] == nullptr)
                pieces[i] = nullptr;
            else
                pieces[i] = copy.pieces[i]->clone();
        }
    }
    Board(Board&& move) noexcept
    {
        for (uint_fast8_t i = 0; i < 64; ++i) {
            pieces[i] = move.pieces[i];
            move.pieces[i] = nullptr;
        }
    }

    Board& operator=(Board&& move)
    {
        for (uint_fast8_t i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)
                delete pieces[i];
            pieces[i] = move.pieces[i];
            move.pieces[i] = nullptr;
        }
        return *this;
    }

    Board& operator=(const Board& copy)
    {
        for (uint_fast8_t i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)
                delete pieces[i];
            if (copy.pieces[i] != nullptr)
                pieces[i] = copy.pieces[i]->clone();
            else
                pieces[i] = nullptr;
        }
        return *this;
    }


    Board()
    {
        for (uint_fast8_t i = 0; i < 64; ++i) {
            pieces[i] = nullptr;//new Piece();
        }
    }

    ~Board()
    {
        for (uint_fast8_t i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)
                delete pieces[i];
        }
    }

    double priceInLocation(char column, char row, char playerColor) const
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            return -1;
        else if (pieces[(column - 'a') + (row - '1') * 8] == nullptr)
            return 0;
        else if (pieces[(column - 'a') + (row - '1') * 8]->occupancy() == playerColor)
            return -1;
        else
            return pieces[(column - 'a') + (row - '1') * 8]->price(row);
    }
    Piece* pieceAt(char column, char row)
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            return nullptr;
        else
            return pieces[(column - 'a') + (row - '1') * 8];
    }

    void setPieceAt(char column, char row, Piece* p)
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            return;
        else
            pieces[(column - 'a') + (row - '1') * 8] = p;
    }

    void deleteAndOverwritePiece(char column, char row, Piece* p)
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            throw new exception("Invalid coordinates");

        delete pieces[(column - 'a') + (row - '1') * 8];
        pieces[(column - 'a') + (row - '1') * 8] = p;
    }

    void deleteAndMovePiece(char columnFrom, char rowFrom, char columnTo, char rowTo)
    {
        if (columnFrom < 'a' || columnFrom>'h' || rowFrom < '1' || rowFrom>'8' || columnTo < 'a' || columnTo>'h' || rowTo < '1' || rowTo>'8')
            throw exception("Invalid coordinates");

        const uint_fast8_t posFrom = (columnFrom - 'a') + (rowFrom - '1') * 8;
        const uint_fast8_t posTo = (columnTo - 'a') + (rowTo - '1') * 8;

        if (pieces[posFrom] == nullptr)
            throw exception("Trying to move empty field");

        if(pieces[posTo] != nullptr)
            delete pieces[posTo];
        pieces[posTo] = pieces[posFrom];
        pieces[posFrom] = nullptr;
    }

    void print() const
    {
        for (int i = 7; i >= 0; --i) {
            wcout << i + 1 << ' ';
            for (int j = 0; j < 8; ++j) {
                wcout << '|';
                //wcout<<((((j+i*8)%2)==0)?"\033[40m":"\033[43m");
                if (pieces[j + i * 8] != nullptr)
                    wcout << pieces[j + i * 8]->print();
                else
                    wcout << L' ';
            }
            wcout << '|' << endl;
        }
        wcout << "   a b c d e f g h";
        /*
        for (char j = 'a'; j <= 'h'; ++j) {
            wcout<<j<<L' ';
        }*/
        wcout << endl;
    }

    /*
        string printBasic() const
        {
            string res;
            for (int i = 0; i < 64; ++i) {
                res+=pieces[i]->printBasic();
            }
            return res;
        }
    */
    double balance() const {
        double res = 0;
        for (int i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)
                res += pieces[i]->price(i / 8) * pieces[i]->occupancy();
        }

        return res;
    }

    double positionScore() {
        double res = 0;
        for (int_fast8_t i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)
            {
                double alpha, beta;
                alpha = -DBL_MAX;
                beta = DBL_MAX;
                uint_fast16_t totalMoves = 0;
                double totalValues = 0;
                pieces[i]->bestPosition(*this, (i & 0b111) + 'a', (i >> 3) + '1', 1, alpha, beta, totalMoves, totalValues, 0,true);

                res += ((double)totalMoves) * pieces[i]->occupancy();
                if(totalMoves>0)
                    res += min(max(totalValues,-100.0),100.0);
            }
        }
        return res / 10000.0;
    }


    double bestPositionScore(int_fast8_t depth, char onMove, double valueSoFar, double alpha, double beta)
    {
        if (itsTimeToStop)
            return 0;


        double bestValue = INT32_MAX * onMove * (-1);
        double totalValues = 0;


        uint_fast16_t totalMoves = 0;
        int depthToPieces = depth;

        for (int_fast8_t i = 0; i < 64; ++i) {
            Piece* found = pieces[i];

            if (found == nullptr)
                continue;
            if (found->occupancy() == onMove)
            {
                auto foundVal = found->bestPosition(*this, (i & 0b111) + 'a', (i >> 3) + '1', depthToPieces, alpha, beta, totalMoves, totalValues, valueSoFar);

                if (foundVal * onMove > bestValue * onMove) {
                    bestValue = foundVal;
                }
                if (foundVal * onMove == kingPrice)//Je možné vzít krále, hra skončila
                {
                    //wcout << endl;
                    //print();
                    //break;
                    return foundVal;//*depth;
                }
                if (beta <= alpha)
                {
                    break;
                    //depthToPieces = 0;
                }
            }
        }

        if (saveToVector)
            return -DBL_MAX;


        if ((int)bestValue == INT32_MAX * onMove * (-1))//Nemůžu udělat žádný legitimní tah (pat nebo mat)
        {
            if ((int)(bestPositionScore(1, onMove * (-1),valueSoFar, alpha, beta) * onMove * (-1)) == kingPrice)//Soupeř je v situaci, kdy mi může vzít krále (ohrožuje ho)
            {
                bestValue = -100000 * depth * onMove;//Dostanu mat, co nejnižší skóre
            }

            else
            {//Dostal bych pat, ten je vždycky lepší než dostat mat, ale chci ho docílit jen když prohrávám
                if (valueSoFar * onMove > 0)
                    bestValue = -500 * depth * onMove;//Prohrávám nebo nastejno, dát remízu beru jako super tah
                else
                    bestValue = 500 * depth * onMove;//Vedu, nechci dostat remízu
            }

        }


        return bestValue;
    }

    



    double tryPiece(char column, char row, Piece* p, int_fast8_t depth, int_fast8_t occupancyToTry, double alpha, double beta, double valueSoFar)
    {
        Piece* backup = pieceAt(column, row);
        setPieceAt(column, row, p);
        auto foundVal = bestPositionScore(depth, occupancyToTry, valueSoFar, alpha, beta);
        setPieceAt(column, row, backup);
        return foundVal;
    }

    double tryPiecePosition(char column, char row, Piece* p)
    {
        Piece* backup = pieceAt(column, row);
        setPieceAt(column, row, p);
        auto foundVal = positionScore();
        setPieceAt(column, row, backup);
        return foundVal;
    }

};


struct BoardWithValues {
    Board board;
    double bestFoundValue;
    double pieceTakenValue;

    BoardWithValues(const BoardWithValues& copy):board(copy.board),bestFoundValue(copy.bestFoundValue),pieceTakenValue(copy.pieceTakenValue){}
    BoardWithValues(BoardWithValues&& toMove) noexcept :board(move(toMove.board)), bestFoundValue(toMove.bestFoundValue), pieceTakenValue(toMove.pieceTakenValue) {}


    //BoardWithValues& operator=(BoardWithValues&& toMove) = default;

    BoardWithValues& operator=(const BoardWithValues& copy) = default;

    friend bool operator<(const BoardWithValues& l, const BoardWithValues& r)
    {
        return l.bestFoundValue < r.bestFoundValue;
    }

    BoardWithValues(Board board, double bestFoundValue, double pieceTakenValue):board(move(board)),bestFoundValue(bestFoundValue),pieceTakenValue(pieceTakenValue) {}
};


vector<BoardWithValues> firstPositions;


void Piece::tryChangeAndUpdateIfBetter(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, double& bestValue, double& totalValues, uint_fast16_t& totalMoves, bool& doNotContinue, double valueSoFar, Piece* changeInto, double minPriceToTake, double maxPriceToTake)
{
    if (doNotContinue && !evaluateMoves)
        return;

    double price = board.priceInLocation(column, row, occupancy());

    //uint_fast16_t moves = 0;

    if (price >= minPriceToTake && price <= maxPriceToTake)
    {
        //int test = totalMoves;
        totalMoves += 1;
        
        totalValues += price * occupancy();
        if (doNotContinue)
            return;

        if (price == kingPrice)//Je možné vzít krále
        {
            doNotContinue = true;
            bestValue = price * occupancy();
            //totalMoves++;
            return;
        }

        if (!(price == 0 || price == -0))
        {
            //const int_fast8_t howFarFromInitial = (depthW - depth);
            const double howFarFromInitialAfterExchange = ((depthW - depth) >> 1);//Kolikaty tah od initial pozice (od 0), ne pultah
            price /= (1 + howFarFromInitialAfterExchange / 100.0);
        }

        double foundVal = 0;
        double valueWithThisPiece = valueSoFar + (price * occupancy());//price * occupancy();


        if (changeInto == nullptr)
            changeInto = this;


        if (saveToVector)
        {
            Piece* backup = board.pieceAt(column, row);
            board.setPieceAt(column, row, changeInto);
            firstPositions.emplace_back(board, price * occupancy(), price * occupancy());
            board.setPieceAt(column, row, backup);
            return;
        }



        if (depth > 0)
        {
            double foundOnly = board.tryPiece(column, row, changeInto, depth, occupancy() * (-1), alpha, beta, valueWithThisPiece);


            foundVal = foundOnly;

            if (((int)foundOnly * occupancy() * (-1)) == kingPrice)//V dalším tahu bych přišel o krále, není to legitimní tah
            {
                //if (foundVal > 0)
                totalMoves -= 1;
                return;
            }
        }
        else//konec tady, list
        {
            foundVal = valueWithThisPiece;
            if(evaluateMoves)
                foundVal+=board.tryPiecePosition(column, row, changeInto);
        }


        if (foundVal * occupancy() > bestValue * occupancy())
            bestValue = foundVal;


        
        if (occupancy() > 0)//bily, maximalizuje hodnotu
        {
            alpha = max(alpha, bestValue);
            //if (bestValue >= beta)
              //  doNotContinue = true;
        }
        else
        {
            beta = min(beta, bestValue);
            //if (bestValue <= alpha)
              //  doNotContinue = true;
        }


        if (beta <= alpha)
        {
            doNotContinue = true;
        }
    }
}


struct Pawn : public Piece {
    //virtual bool canBeEvolved(char row) const = 0;
    virtual char evolveRow() const = 0;

    virtual vector<Piece*>* evolveIntoReference(char row) const = 0;

    virtual char advanceRow() const = 0;

    virtual bool canGoTwoFields(char row) const = 0;

    virtual double bestPosition(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override
    {
        double bestValue = INT32_MAX * (-1) * occupancy();

        uint_fast16_t moves = 0;


        //if (depth <= 0)
          //  return 0;

        auto availableOptions = evolveIntoReference(row + advanceRow());


        //double rowAdvantage = 0;
        //if(availableOptions.size()==1)
        //{
            //rowAdvantage = (6-abs(row-evolveRow()))/128.0;
            /*
            bool allLineClear = true;
            for (char i = row; i != evolveRow(); i+=advanceRow()) {
                if(board.priceInLocation(column,i,0)!=0)
                {
                    allLineClear=false;

                }
            }*/
            // }


        board.setPieceAt(column, row, nullptr);

        if (board.priceInLocation(column, row + advanceRow(), occupancy()) == 0)//Muzu jit pescem o jedno dopredu
        {

            for (int_fast8_t i = 0; i < availableOptions->size(); ++i) {

                tryChangeAndUpdateIfBetter(board, column, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar,(*availableOptions)[i], 0, 0);
                if (canGoTwoFields(row))
                    tryChangeAndUpdateIfBetter(board, column, row + advanceRow() * 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar, (*availableOptions)[i], 0, 0);

            }
        }
        for (int_fast8_t i = 0; i < availableOptions->size(); ++i) {
            tryChangeAndUpdateIfBetter(board, column + 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar, (*availableOptions)[i], DBL_MIN);
            tryChangeAndUpdateIfBetter(board, column - 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar, (*availableOptions)[i], DBL_MIN);
        }
        board.setPieceAt(column, row, this);


        return bestValue;
    }

};


struct Knight : public Piece {

    virtual double bestPosition(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {

        double bestValue = INT32_MAX * (-1) * occupancy();

        //if (depth <= 0)
          //  return 0;
        board.setPieceAt(column, row, nullptr);

        tryChangeAndUpdateIfBetter(board, column + 1, row + 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 1, row - 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 2, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 2, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row + 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row - 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 2, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 2, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);

        board.setPieceAt(column, row, this);

        return bestValue;
    }


    virtual double price(char row) const override {
        return 3.2;
    }
};


struct Bishop : public Piece {
    virtual double bestPosition(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue)  override {

        double bestValue = INT32_MAX * (-1) * occupancy();
        //if (depth <= 0)
          //  return 0;

        board.setPieceAt(column, row, nullptr);
        //bool foundKing = false;

        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column + i, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column + i, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column - i, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column - i, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }


        board.setPieceAt(column, row, this);


        return bestValue;
    }


    virtual double price(char row) const override {
        return 3.33;
    }
};


struct Rook : public Piece {
    virtual double bestPosition(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {

        double bestValue = INT32_MAX * (-1) * occupancy();

        board.setPieceAt(column, row, nullptr);

        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column + i, row, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column - i, row, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }


        board.setPieceAt(column, row, this);

        return bestValue;
    }


    virtual double price(char row) const override {
        return 5.1;
    }
};


struct Queen : public Piece {

    virtual double bestPosition(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {
        //board.print();

        double bestValue = INT32_MAX * (-1) * occupancy();


        Piece* originalPiece = board.pieceAt(column, row);
        board.setPieceAt(column, row, nullptr);



        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column + i, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column + i, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column - i, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column - i, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }

        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column + i, row, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }



        for (char i = 1; i < 8; ++i) {
            double price = board.priceInLocation(column - i, row, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
                if (price > 0)
                    break;
            }
            else
                break;
        }

        board.setPieceAt(column, row, this);


        return bestValue;
    }


    virtual double price(char row) const override {
        return 8.8;
    }
};

struct King : public Piece {
    virtual double bestPosition(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {
        double bestValue = INT32_MAX * (-1) * occupancy();

        board.setPieceAt(column, row, nullptr);

        tryChangeAndUpdateIfBetter(board, column + 1, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 1, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 1, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);

        board.setPieceAt(column, row, this);

        return bestValue;

    }


    double price(char row) const override {
        return kingPrice;
    }
};

struct KnightWhite :public Knight {
    char occupancy() const override {
        return 1;
    }
    wchar_t print() const override {
        return L'♘';
    }

    Piece* clone() const override {
        return new KnightWhite(*this);
    }
    char printBasic() const override {
        return 'n';
    }
};

struct KnightBlack :public Knight {
    char occupancy() const override {
        return -1;
    }
    wchar_t print() const override {
        return L'♞';
    }
    Piece* clone() const override {
        return new KnightBlack(*this);
    }
    char printBasic() const override {
        return 'N';
    }
};

struct BishopWhite :public Bishop {
    char occupancy() const override {
        return 1;
    }
    wchar_t print() const override {
        return L'♗';
    }
    Piece* clone() const override {
        return new BishopWhite(*this);
    }
    char printBasic() const override {
        return 'b';
    }
};

struct BishopBlack :public Bishop {
    char occupancy() const override {
        return -1;
    }
    wchar_t print() const override {
        return L'♝';
    }
    Piece* clone() const override {
        return new BishopBlack(*this);
    }
    char printBasic() const override {
        return 'B';
    }
};

struct RookWhite :public Rook {
    char occupancy() const override {
        return 1;
    }
    wchar_t print() const override {
        return L'♖';
    }
    Piece* clone() const override {
        return new RookWhite(*this);
    }
    char printBasic() const override {
        return 'r';
    }
};

struct RookBlack :public Rook {
    char occupancy() const override {
        return -1;
    }
    wchar_t print() const override {
        return L'♜';
    }
    Piece* clone() const override {
        return new RookBlack(*this);
    }
    char printBasic() const override {
        return 'R';
    }
};
struct QueenWhite :public Queen {
    char occupancy() const override {
        return 1;
    }
    wchar_t print() const override {
        return L'♕';
    }
    Piece* clone() const override {
        return new QueenWhite(*this);
    }
    char printBasic() const override {
        return 'q';
    }
};
struct QueenBlack :public Queen {
    char occupancy() const override {
        return -1;
    }
    wchar_t print() const override {
        return L'♛';
    }
    Piece* clone() const override {
        return new QueenBlack(*this);
    }
    char printBasic() const override {
        return 'Q';
    }
};
struct KingWhite :public King {
    char occupancy() const override {
        return 1;
    }
    wchar_t print() const override {
        return L'♔';
    }
    Piece* clone() const override {
        return new KingWhite(*this);
    }
    char printBasic() const override {
        return 'k';
    }
};
struct KingBlack :public King {
    char occupancy() const override {
        return -1;
    }
    wchar_t print() const override {
        return L'♚';
    }
    Piece* clone() const override {
        return new KingBlack(*this);
    }
    char printBasic() const override {
        return 'K';
    }
};


struct PawnWhite :public Pawn {
    char occupancy() const override {
        return 1;
    }
    wchar_t print() const override {
        return L'♙';
    }
    char evolveRow() const override {
        return '8';
    }

    /*bool canBeEvolved(char row) const override{
        return row=='8';
    }*/

    bool canGoTwoFields(char row) const override {
        return row == '2';
    }

    char advanceRow() const override {
        return 1;
    }

    Piece* clone() const override {
        return new PawnWhite(*this);
    }
    char printBasic() const override {
        return 'p';
    }


    double price(char row) const override {
        //return 1;
        switch (row) {
        case '5':
            return 1.5;
        case '6':
            return 2;
        case '7':
            return 3;
        default:
            return 1;

        }
    }
    virtual vector<Piece*>* evolveIntoReference(char row) const;

};
struct PawnBlack :public Pawn {
    char occupancy() const override {
        return -1;
    }
    wchar_t print() const override {
        return L'♟';
    }
    char evolveRow() const override {
        return '1';
    }
    /*bool canBeEvolved(char row) const override{
        return row=='1';
    }*/
    bool canGoTwoFields(char row) const override {
        return row == '7';
    }
    char advanceRow() const override {
        return -1;
    }

    Piece* clone() const override {
        return new PawnBlack(*this);
    }
    char printBasic() const override {
        return 'P';
    }

    double price(char row) const override {
        //return 1;
        switch (row) {
        case '4':
            return 1.5;
        case '3':
            return 2;
        case '2':
            return 3;
        default:
            return 1;

        }
    }

    virtual vector<Piece*>* evolveIntoReference(char row) const;

};
vector<Piece*> whiteEvolvePawnOnly = { new PawnWhite() };
vector<Piece*> whiteEvolveLastRow = { new QueenWhite(), new RookWhite(), new BishopWhite(), new KnightWhite() };
vector<Piece*> blackEvolvePawnOnly = { new PawnBlack() };
vector<Piece*> blackEvolveLastRow = { new QueenBlack(), new RookBlack(), new BishopBlack(), new KnightBlack() };

vector<Piece*>* PawnWhite::evolveIntoReference(char row) const {
    if (row == evolveRow())
        return &whiteEvolveLastRow;
    else
        return &whiteEvolvePawnOnly;
}

vector<Piece*>* PawnBlack::evolveIntoReference(char row) const {
    if (row == evolveRow())
        return &blackEvolveLastRow;
    else
        return &blackEvolvePawnOnly;
}



atomic<double> alphaOrBeta;


class ToProcessQueue
{
    std::mutex m;
    std::queue<BoardWithValues*> q;
    condition_variable cv_empty;
public:
    void push(BoardWithValues* item)
    {
        unique_lock<mutex> ul(m);
        q.push(move(item));
        cv_empty.notify_one();
    }
    BoardWithValues* pop()
    {
        unique_lock<mutex> ul(m);
        cv_empty.wait(ul, [this]() { return (!q.empty()); });
        auto res = q.front();
        q.pop();
        return res;
    }

};

ToProcessQueue q;

void findOutArgument(BoardWithValues* board, int_fast8_t depth, char onMove)//, double alpha = -DBL_MAX, double beta = DBL_MAX)
{
    Board toDestroy(board->board);

    if (onMove < 0)
    {
        board->bestFoundValue= toDestroy.bestPositionScore(depth, onMove, board->pieceTakenValue, alphaOrBeta, DBL_MAX);
        alphaOrBeta = max(alphaOrBeta * 1.0, board->bestFoundValue * 1.0);
    }
    else
    {
        board->bestFoundValue = toDestroy.bestPositionScore(depth, onMove,board->pieceTakenValue, -DBL_MAX, alphaOrBeta);
        alphaOrBeta = min(alphaOrBeta * 1.0, board->bestFoundValue * 1.0);
    }


    //wcout << "New alpha/beta: " << alphaOrBeta << endl;
}


void workerFromQ()//, double alpha = -DBL_MAX, double beta = DBL_MAX)
{
    while (true)
    {
        auto tmp = q.pop();
       
        if (tmp->bestFoundValue == -DBL_MAX)
            return;
        findOutArgument(tmp, depthW, onMoveW);
    }
}

auto stopper = BoardWithValues(Board(), -DBL_MAX, -DBL_MAX);



pair<Board, double> findBestOnSameLevel(vector<BoardWithValues>& boards, int_fast8_t depth, char onMove)
{
    const int_fast8_t threadCount = 4;//thread::hardware_concurrency(); //4;
    vector<thread> threads;

    if (onMove < 0)
        alphaOrBeta = -DBL_MAX;
    else
        alphaOrBeta = DBL_MAX;

    itsTimeToStop = false;
    onMoveW = onMove;
    depthW = depth;

    threads.reserve(threadCount);
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(workerFromQ);
    }

    if (onMove < 0)
    {
        for (auto it = boards.rbegin(); it != boards.rend(); it++) {
            q.push( &(*it) );
        }
    }
    else
    {
        for (auto& board : boards) {
            q.push(&board);
        }
    }


    for (int i = 0; i < threadCount; ++i) {
        q.push(&stopper);
    }

    for (int i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    std::stable_sort(boards.begin(), boards.end());

    if (!itsTimeToStop)
    {
        for (int i = 0; i < boards.size(); ++i) {
            wcout << boards[i].bestFoundValue << endl;
            wcout << boards[i].pieceTakenValue << endl;
            boards[i].board.print();
        }
        wcout << endl << endl;
    }
    


    if (onMove == 1)
    {
        return { boards[0].board,boards[0].bestFoundValue };
    }
    else
    {
        return { boards[boards.size() - 1].board,boards[boards.size() - 1].bestFoundValue };
    }

}

void timeLimit(int milliseconds)
{
    this_thread::sleep_for(chrono::milliseconds(milliseconds));
    itsTimeToStop = true;
}

vector<BoardWithValues> allBoardsFromPosition(Board& board, char onMove)
{
    itsTimeToStop = false;
    onMoveW = onMove;
    depthW = 1;
    saveToVector = true;
    board.bestPositionScore(1, onMove, 0, -DBL_MAX, DBL_MAX);
    saveToVector = false;
    auto res = firstPositions;
    firstPositions.clear();
    std::stable_sort(res.begin(), res.end());
    return res;
}

pair<Board, double> findBestOnTopLevel(Board& board, int_fast8_t depth, char onMove)
{
    auto tmp = allBoardsFromPosition(board, onMove);
    return findBestOnSameLevel(move(tmp), depth - 1, onMove * (-1));
}

pair<Board, double> findBestInTimeLimit(Board& board, char onMove, int milliseconds)
{
    evaluateMoves = false;
    auto limit = thread(timeLimit, milliseconds);

    auto boardList = allBoardsFromPosition(board, onMove);
    //for (size_t i = 0; i < boardList.size(); i++)
    //{
    //    wcout << boardList[i].pieceTakenValue << endl;
    //    boardList[i].board.print();
    //}

    pair<Board, double> res;
    evaluateMoves = true;

    wcout << "Depth: ";
    for (int_fast8_t i = 3; i < 100; i += 2) {
        auto bestPosFound = findBestOnSameLevel(boardList, i, onMove * (-1));
        evaluateMoves = false;
        if (itsTimeToStop)
            break;
        else
        {
            res = bestPosFound;
        }
        wcout << i + 1 << ' ';
        
    }
    wcout << endl;
    limit.join();

    return res;
}

pair<Board, double> findBestInNumberOfMoves(Board& board, char onMove, char moves)
{
    evaluateMoves = false;
    auto boardList = allBoardsFromPosition(board, onMove);

    pair<Board, double> res;
    evaluateMoves = true;

    wcout << "Depth: ";
    for (int_fast8_t i = 3; i < moves; i += 2) {
        auto bestPosFound = findBestOnSameLevel(boardList, i, onMove * (-1));
        evaluateMoves = false;
        if (itsTimeToStop)
            break;
        else
        {
            res = bestPosFound;
        }
        wcout << i + 1 << ' ';
    }
    wcout << endl;

    return res;
}

Board startingPosition()
{
    Board initial;

    initial.deleteAndOverwritePiece('a', '1', new RookWhite());
    initial.deleteAndOverwritePiece('b', '1', new KnightWhite());
    initial.deleteAndOverwritePiece('c', '1', new BishopWhite());
    initial.deleteAndOverwritePiece('d', '1', new QueenWhite());
    initial.deleteAndOverwritePiece('e', '1', new KingWhite());
    initial.deleteAndOverwritePiece('f', '1', new BishopWhite());
    initial.deleteAndOverwritePiece('g', '1', new KnightWhite());
    initial.deleteAndOverwritePiece('h', '1', new RookWhite());

    initial.deleteAndOverwritePiece('a', '2', new PawnWhite());
    initial.deleteAndOverwritePiece('b', '2', new PawnWhite());
    initial.deleteAndOverwritePiece('c', '2', new PawnWhite());
    initial.deleteAndOverwritePiece('d', '2', new PawnWhite());
    initial.deleteAndOverwritePiece('e', '2', new PawnWhite());
    initial.deleteAndOverwritePiece('f', '2', new PawnWhite());
    initial.deleteAndOverwritePiece('g', '2', new PawnWhite());
    initial.deleteAndOverwritePiece('h', '2', new PawnWhite());

    initial.deleteAndOverwritePiece('a', '8', new RookBlack());
    initial.deleteAndOverwritePiece('b', '8', new KnightBlack());
    initial.deleteAndOverwritePiece('c', '8', new BishopBlack());
    initial.deleteAndOverwritePiece('d', '8', new QueenBlack());
    initial.deleteAndOverwritePiece('e', '8', new KingBlack());
    initial.deleteAndOverwritePiece('f', '8', new BishopBlack());
    initial.deleteAndOverwritePiece('g', '8', new KnightBlack());
    initial.deleteAndOverwritePiece('h', '8', new RookBlack());
    initial.deleteAndOverwritePiece('h', '7', new RookWhite());

    initial.deleteAndOverwritePiece('a', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('b', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('c', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('d', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('e', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('f', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('g', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('h', '7', new PawnBlack());
    return initial;
}

void benchmark(char depth = 8, Board board = startingPosition(), char onMove = 1)
{
    board.print();
    auto start = chrono::high_resolution_clock::now();
    //doneMoves.clear();
    //auto result = findBestOnTopLevel(board,depth,onMove);
    //auto result = findBestInTimeLimit(board, onMove, timeToPlay);
    auto result = findBestInNumberOfMoves(board, onMove, depth);
    auto end = chrono::high_resolution_clock::now();
    board = result.first;
    wcout << "Best position found score change: " << result.second << endl;//<<"Total found score "<<result.second+result.first.balance()<<endl;


    auto elapsed = chrono::duration_cast<chrono::milliseconds>(end - start).count() / 1000.0;
    wcout << "Done in " << elapsed << "s." << endl;

    board.print();
}


long long boardUserInput(Board& board)
{
    board.print();
    string tmp;
    auto startHuman = chrono::high_resolution_clock::now();
    cin >> tmp;
    auto endHuman = chrono::high_resolution_clock::now();

    //char columnOrig,rowOrig,columnTo,rowTo;
//cin>>columnOrig;
//cin>>rowOrig;
//cin>>columnTo;
//cin>>rowTo;

//initial.movePiece(columnOrig,rowOrig,columnTo,rowTo);

    while (true)
    {
        try
        {
            if (tmp.length()<4)
                throw exception("bad input");
            if (tmp[0] == 'w' && tmp[1] == 'q')
                board.deleteAndOverwritePiece(tmp[2], tmp[3], new QueenWhite());
            else if (tmp[0] == 'b' && tmp[1] == 'q')
                board.deleteAndOverwritePiece(tmp[2], tmp[3], new QueenBlack());
            else if (tmp[0] == 'w' && tmp[1] == 'r')
                board.deleteAndOverwritePiece(tmp[2], tmp[3], new RookWhite());
            else if (tmp[0] == 'b' && tmp[1] == 'r')
                board.deleteAndOverwritePiece(tmp[2], tmp[3], new RookBlack());
            else if (tmp[0] == 'w' && tmp[1] == 'b')
                board.deleteAndOverwritePiece(tmp[2], tmp[3], new BishopWhite());
            else if (tmp[0] == 'b' && tmp[1] == 'b')
                board.deleteAndOverwritePiece(tmp[2], tmp[3], new BishopBlack());
            else if (tmp[0] == 'w' && tmp[1] == 'k')
                board.deleteAndOverwritePiece(tmp[2], tmp[3], new KnightWhite());
            else if (tmp[0] == 'b' && tmp[1] == 'k')
                board.deleteAndOverwritePiece(tmp[2], tmp[3], new KnightBlack());
            else
            {
                board.deleteAndMovePiece(tmp[0], tmp[1], tmp[2], tmp[3]);
                if (tmp.size() > 4)
                    board.deleteAndMovePiece(tmp[5], tmp[6], tmp[7], tmp[8]);
            }

            break;
        }
        catch (const std::exception& e)
        {
            std::wcout << e.what() << endl;
            cin >> tmp;
        }
    }


    auto elapsedHuman = chrono::duration_cast<chrono::milliseconds>(endHuman - startHuman).count();
    wcout << "Human moved in " << elapsedHuman / 1000.0 << "s." << endl;

    board.print();
    return elapsedHuman + 10;
}


void playGameInTime(Board board, char onMove, int timeToPlay)
{
    board.print();
    if (onMove < 0)
    {
        boardUserInput(board);
    }
    
    while (true)
    {
        auto start = chrono::high_resolution_clock::now();
        auto result = findBestInTimeLimit(board, onMove, timeToPlay);
        board = result.first;
        wcout << "Best position found score change: " << result.second << endl;//<<"Total found score "<<result.second+result.first.balance()<<endl;
        auto end = chrono::high_resolution_clock::now();

        auto elapsed = chrono::duration_cast<chrono::milliseconds>(end - start).count() / 1000.0;
        wcout << "Done in " << elapsed << "s." << endl;

        boardUserInput(board);

    }
}



void playGameResponding(Board board, char onMove)
{
    long long milliseconds = 5000;
    if (onMove < 0)
    {
        milliseconds = boardUserInput(board);
    }

    while (true)
    {
        auto start = chrono::high_resolution_clock::now();
        //doneMoves.clear();
        auto result = findBestInTimeLimit(board, onMove, milliseconds);
        //auto result = findBestInNumberOfMoves(board, onMove, 8);
        auto end = chrono::high_resolution_clock::now();
        board = result.first;
        wcout << "Best position found score change: " << result.second << endl;//<<"Total found score "<<result.second+result.first.balance()<<endl;

        auto elapsed = chrono::duration_cast<chrono::milliseconds>(end - start).count() / 1000.0;
        wcout << "PC answered in " << elapsed << "s." << endl;

        milliseconds = boardUserInput(board);
    }
}


int main() {
    std::wcout.sync_with_stdio(false);
    init_locale();

    Board initialGreedyCheckmate(startingPosition());
    initialGreedyCheckmate.deleteAndMovePiece('f', '1', 'c', '6');
    initialGreedyCheckmate.deleteAndMovePiece('e', '2', 'e', '3');
    initialGreedyCheckmate.deleteAndMovePiece('d', '7', 'd', '6');


    Board endgame;
    endgame.deleteAndOverwritePiece('a', '4', new PawnWhite());
    endgame.deleteAndOverwritePiece('d', '4', new PawnWhite());
    endgame.deleteAndOverwritePiece('g', '3', new QueenBlack());
    endgame.deleteAndOverwritePiece('b', '5', new BishopBlack());
    endgame.deleteAndOverwritePiece('d', '5', new PawnBlack());
    endgame.deleteAndOverwritePiece('b', '6', new PawnBlack());
    endgame.deleteAndOverwritePiece('a', '7', new PawnBlack());
    endgame.deleteAndOverwritePiece('c', '7', new PawnBlack());
    endgame.deleteAndOverwritePiece('f', '5', new KingWhite());
    endgame.deleteAndOverwritePiece('e', '6', new PawnWhite());
    endgame.deleteAndOverwritePiece('e', '7', new KingBlack());
    endgame.deleteAndOverwritePiece('g', '7', new PawnBlack());
    endgame.deleteAndOverwritePiece('h', '8', new RookBlack());

    Board endgame2;
    endgame2.deleteAndOverwritePiece('a', '3', new PawnWhite());
    endgame2.deleteAndOverwritePiece('c', '3', new PawnWhite());
    endgame2.deleteAndOverwritePiece('d', '4', new PawnWhite());
    endgame2.deleteAndOverwritePiece('e', '5', new PawnWhite());
    endgame2.deleteAndOverwritePiece('d', '5', new PawnBlack());

    endgame2.deleteAndOverwritePiece('h', '3', new PawnBlack());

    endgame2.deleteAndOverwritePiece('b', '6', new PawnBlack());
    endgame2.deleteAndOverwritePiece('a', '7', new PawnBlack());
    endgame2.deleteAndOverwritePiece('c', '7', new PawnBlack());


    endgame2.deleteAndOverwritePiece('b', '5', new BishopBlack());

    endgame2.deleteAndOverwritePiece('f', '2', new KingWhite());

    endgame2.deleteAndOverwritePiece('e', '7', new KingBlack());
    endgame2.deleteAndOverwritePiece('g', '7', new PawnBlack());
    endgame2.deleteAndOverwritePiece('h', '8', new RookBlack());


    Board endGameMatousPc;
    endGameMatousPc.deleteAndOverwritePiece('b', '8', new KnightBlack());
    endGameMatousPc.deleteAndOverwritePiece('f', '8', new KingBlack());
    endGameMatousPc.deleteAndOverwritePiece('g', '8', new KnightBlack());
    endGameMatousPc.deleteAndOverwritePiece('g', '7', new RookWhite());
    endGameMatousPc.deleteAndOverwritePiece('b', '5', new PawnWhite());
    endGameMatousPc.deleteAndOverwritePiece('d', '5', new KingWhite());
    endGameMatousPc.deleteAndOverwritePiece('h', '5', new PawnBlack());
    endGameMatousPc.deleteAndOverwritePiece('h', '4', new PawnWhite());
    endGameMatousPc.deleteAndOverwritePiece('c', '2', new PawnWhite());
    endGameMatousPc.deleteAndOverwritePiece('g', '2', new PawnWhite());
    endGameMatousPc.deleteAndOverwritePiece('h', '2', new BishopBlack());


    Board endGameMatousPc2;
    endGameMatousPc2.deleteAndOverwritePiece('f', '8', new BishopBlack());
    endGameMatousPc2.deleteAndOverwritePiece('e', '6', new KingWhite());
    endGameMatousPc2.deleteAndOverwritePiece('h', '3', new RookWhite());
    endGameMatousPc2.deleteAndOverwritePiece('e', '1', new KingBlack());
    endGameMatousPc2.deleteAndOverwritePiece('c', '2', new PawnWhite());
    endGameMatousPc2.deleteAndOverwritePiece('g', '2', new PawnWhite());

    Board endGameMatousPc3;
    endGameMatousPc3.deleteAndOverwritePiece('d', '6', new BishopBlack());
    endGameMatousPc3.deleteAndOverwritePiece('e', '6', new KingWhite());
    endGameMatousPc3.deleteAndOverwritePiece('h', '1', new RookWhite());
    endGameMatousPc3.deleteAndOverwritePiece('d', '2', new KingBlack());
    endGameMatousPc3.deleteAndOverwritePiece('c', '5', new PawnWhite());
    endGameMatousPc3.deleteAndOverwritePiece('h', '8', new QueenWhite());


    Board puzzle1;
    puzzle1.deleteAndOverwritePiece('b', '2', new RookBlack());
    puzzle1.deleteAndOverwritePiece('b', '4', new PawnWhite());
    puzzle1.deleteAndOverwritePiece('c', '5', new BishopWhite());
    puzzle1.deleteAndOverwritePiece('d', '1', new KingWhite());
    puzzle1.deleteAndOverwritePiece('d', '3', new BishopWhite());
    puzzle1.deleteAndOverwritePiece('d', '5', new PawnWhite());
    puzzle1.deleteAndOverwritePiece('d', '7', new PawnWhite());
    puzzle1.deleteAndOverwritePiece('f', '2', new PawnBlack());
    puzzle1.deleteAndOverwritePiece('f', '6', new BishopBlack());
    puzzle1.deleteAndOverwritePiece('f', '7', new KingBlack());



    Board klaraHra;
    klaraHra.deleteAndOverwritePiece('a', '7', new PawnBlack());
    klaraHra.deleteAndOverwritePiece('b', '6', new PawnBlack());
    klaraHra.deleteAndOverwritePiece('c', '5', new PawnBlack());
    klaraHra.deleteAndOverwritePiece('d', '5', new PawnBlack());
    klaraHra.deleteAndOverwritePiece('e', '6', new PawnBlack());
    klaraHra.deleteAndOverwritePiece('f', '7', new PawnBlack());
    klaraHra.deleteAndOverwritePiece('g', '6', new PawnBlack());
    klaraHra.deleteAndOverwritePiece('h', '7', new PawnBlack());
    klaraHra.deleteAndOverwritePiece('g', '8', new KingBlack());
    klaraHra.deleteAndOverwritePiece('f', '8', new RookBlack());
    klaraHra.deleteAndOverwritePiece('a', '8', new RookBlack());
    klaraHra.deleteAndOverwritePiece('d', '7', new QueenBlack());
    klaraHra.deleteAndOverwritePiece('c', '6', new KnightBlack());
    klaraHra.deleteAndOverwritePiece('d', '4', new KnightBlack());
    klaraHra.deleteAndOverwritePiece('e', '5', new BishopBlack());

    klaraHra.deleteAndOverwritePiece('a', '2', new PawnWhite());
    klaraHra.deleteAndOverwritePiece('b', '2', new PawnWhite());
    klaraHra.deleteAndOverwritePiece('c', '2', new PawnWhite());
    klaraHra.deleteAndOverwritePiece('d', '3', new PawnWhite());
    klaraHra.deleteAndOverwritePiece('f', '2', new PawnWhite());
    klaraHra.deleteAndOverwritePiece('g', '4', new PawnWhite());
    klaraHra.deleteAndOverwritePiece('h', '2', new PawnWhite());
    klaraHra.deleteAndOverwritePiece('a', '4', new QueenWhite());
    klaraHra.deleteAndOverwritePiece('c', '3', new KnightWhite());
    klaraHra.deleteAndOverwritePiece('f', '3', new KnightWhite());
    klaraHra.deleteAndOverwritePiece('a', '1', new RookWhite());
    klaraHra.deleteAndOverwritePiece('h', '1', new RookWhite());
    klaraHra.deleteAndOverwritePiece('e', '2', new KingWhite());
    klaraHra.deleteAndOverwritePiece('g', '5', new BishopWhite());



    Board klaraHra2;
    klaraHra2.deleteAndOverwritePiece('a', '7', new PawnBlack());
    klaraHra2.deleteAndOverwritePiece('b', '6', new PawnBlack());
    klaraHra2.deleteAndOverwritePiece('c', '5', new PawnBlack());
    klaraHra2.deleteAndOverwritePiece('d', '4', new PawnBlack());
    klaraHra2.deleteAndOverwritePiece('e', '6', new PawnBlack());
    klaraHra2.deleteAndOverwritePiece('f', '7', new PawnBlack());
    klaraHra2.deleteAndOverwritePiece('g', '6', new PawnBlack());
    klaraHra2.deleteAndOverwritePiece('h', '7', new PawnBlack());
    klaraHra2.deleteAndOverwritePiece('f', '7', new PawnBlack());
    klaraHra2.deleteAndOverwritePiece('g', '7', new KingBlack());
    klaraHra2.deleteAndOverwritePiece('d', '8', new RookBlack());
    klaraHra2.deleteAndOverwritePiece('a', '8', new RookBlack());
    klaraHra2.deleteAndOverwritePiece('d', '7', new QueenBlack());
    klaraHra2.deleteAndOverwritePiece('c', '6', new KnightBlack());


    klaraHra2.deleteAndOverwritePiece('a', '2', new PawnWhite());
    klaraHra2.deleteAndOverwritePiece('c', '2', new PawnWhite());
    klaraHra2.deleteAndOverwritePiece('d', '3', new PawnWhite());
    klaraHra2.deleteAndOverwritePiece('f', '2', new PawnWhite());
    klaraHra2.deleteAndOverwritePiece('g', '5', new PawnWhite());
    klaraHra2.deleteAndOverwritePiece('h', '2', new PawnWhite());
    klaraHra2.deleteAndOverwritePiece('a', '4', new QueenWhite());
    klaraHra2.deleteAndOverwritePiece('b', '1', new RookWhite());
    klaraHra2.deleteAndOverwritePiece('h', '1', new RookWhite());
    klaraHra2.deleteAndOverwritePiece('g', '2', new KingWhite());
    //initial.deleteAndOverwritePiece('d','5',new PawnBlack());
    //initial.deleteAndOverwritePiece('d','8',new QueenBlack());


    Board klaraHra3;
    klaraHra3.deleteAndOverwritePiece('h', '7', new PawnBlack());
    klaraHra3.deleteAndOverwritePiece('g', '6', new PawnBlack());
    klaraHra3.deleteAndOverwritePiece('g', '5', new PawnBlack());
    klaraHra3.deleteAndOverwritePiece('h', '8', new KingBlack());
    klaraHra3.deleteAndOverwritePiece('f', '8', new RookBlack());
    klaraHra3.deleteAndOverwritePiece('a', '4', new RookBlack());
    klaraHra3.deleteAndOverwritePiece('d', '1', new QueenBlack());

    klaraHra3.deleteAndOverwritePiece('h', '6', new PawnWhite());
    klaraHra3.deleteAndOverwritePiece('g', '3', new KingWhite());
    klaraHra3.deleteAndOverwritePiece('c', '2', new RookWhite());



    Board testMatu;
    testMatu.deleteAndOverwritePiece('h', '8', new KingBlack());
    testMatu.deleteAndOverwritePiece('a', '1', new KingWhite());
    testMatu.deleteAndOverwritePiece('g', '1', new RookWhite());
    testMatu.deleteAndOverwritePiece('a', '7', new RookWhite());
    testMatu.deleteAndOverwritePiece('b', '1', new QueenWhite());
    testMatu.deleteAndOverwritePiece('c', '7', new PawnWhite());

    Board testMatuDama;
    testMatuDama.deleteAndOverwritePiece('h', '8', new KingBlack());
    testMatuDama.deleteAndOverwritePiece('e', '8', new KingWhite());
    testMatuDama.deleteAndOverwritePiece('b', '1', new QueenWhite());


    Board testMatu2;
    testMatu2.setPieceAt('a', '8', new RookBlack());
    testMatu2.setPieceAt('e', '8', new KingBlack());
    testMatu2.setPieceAt('e', '8', new KingBlack());
    testMatu2.setPieceAt('c', '7', new PawnBlack());
    testMatu2.setPieceAt('e', '7', new KnightBlack());
    testMatu2.setPieceAt('f', '7', new PawnBlack());
    testMatu2.setPieceAt('g', '7', new PawnBlack());
    testMatu2.setPieceAt('a', '6', new PawnBlack());
    testMatu2.setPieceAt('a', '5', new PawnWhite());
    testMatu2.setPieceAt('d', '5', new PawnBlack());
    testMatu2.setPieceAt('e', '5', new KingWhite());
    testMatu2.setPieceAt('f', '5', new BishopBlack());
    testMatu2.setPieceAt('c', '4', new PawnBlack());
    testMatu2.setPieceAt('h', '2', new RookBlack());


    Board initial;

    initial.deleteAndOverwritePiece('a', '1', new RookWhite());
    initial.deleteAndOverwritePiece('b', '3', new KnightWhite());
    //initial.deleteAndOverwritePiece('c', '1', new BishopWhite());
    initial.deleteAndOverwritePiece('d', '1', new QueenWhite());
    initial.deleteAndOverwritePiece('e', '2', new KingWhite());
    initial.deleteAndOverwritePiece('f', '1', new BishopWhite());
    initial.deleteAndOverwritePiece('f', '3', new KnightWhite());
    initial.deleteAndOverwritePiece('h', '1', new RookWhite());

    initial.deleteAndOverwritePiece('a', '3', new PawnWhite());
    initial.deleteAndOverwritePiece('b', '4', new PawnWhite());
    initial.deleteAndOverwritePiece('c', '4', new PawnWhite());
    initial.deleteAndOverwritePiece('c', '5', new PawnWhite());
    initial.deleteAndOverwritePiece('e', '3', new PawnWhite());
    initial.deleteAndOverwritePiece('f', '2', new PawnWhite());
    initial.deleteAndOverwritePiece('g', '2', new PawnWhite());
    initial.deleteAndOverwritePiece('h', '2', new PawnWhite());

    initial.deleteAndOverwritePiece('a', '8', new RookBlack());
    initial.deleteAndOverwritePiece('b', '8', new KnightBlack());
    initial.deleteAndOverwritePiece('c', '8', new BishopBlack());
    initial.deleteAndOverwritePiece('b', '2', new QueenBlack());
    initial.deleteAndOverwritePiece('e', '8', new KingBlack());
    //initial.deleteAndOverwritePiece('f', '8', new BishopBlack());
    initial.deleteAndOverwritePiece('e', '7', new KnightBlack());
    initial.deleteAndOverwritePiece('h', '8', new RookBlack());
    //initial.deleteAndOverwritePiece('h', '7', new RookWhite());

    initial.deleteAndOverwritePiece('a', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('b', '7', new PawnBlack());
    //initial.deleteAndOverwritePiece('c', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('d', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('e', '6', new PawnBlack());
    initial.deleteAndOverwritePiece('f', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('g', '7', new PawnBlack());
    initial.deleteAndOverwritePiece('h', '5', new PawnBlack());

    Board lossOfQueenPossible;
    lossOfQueenPossible.deleteAndOverwritePiece('a', '1', new RookWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('e', '1', new KingWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('h', '1', new RookWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('b', '2', new PawnWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('f', '2', new PawnWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('h', '2', new PawnWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('c', '3', new KnightWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('d', '3', new PawnWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('f', '3', new QueenWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('c', '4', new PawnWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('b', '5', new PawnWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('d', '5', new PawnWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('f', '5', new BishopWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('h', '4', new KnightWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('h', '6', new BishopWhite());
    lossOfQueenPossible.deleteAndOverwritePiece('a', '5', new PawnBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('a', '7', new RookBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('c', '8', new BishopBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('d', '7', new KnightBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('e', '8', new QueenBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('f', '7', new KingBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('f', '8', new BishopBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('h', '8', new RookBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('g', '6', new PawnBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('h', '7', new PawnBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('e', '7', new PawnBlack());
    lossOfQueenPossible.deleteAndOverwritePiece('c', '7', new PawnBlack());

    Board checkTest(startingPosition());
    checkTest.deleteAndOverwritePiece('a', '4',new BishopWhite());
    checkTest.deleteAndOverwritePiece('e', '6', new QueenWhite());
    checkTest.deleteAndOverwritePiece('f', '7', nullptr);

    //klaraHra.bestPosition(6,1);
    //playGameResponding(startingPosition(), -1);
    //benchmark();
    //playGameResponding(endgame2, 1);
    benchmark(8, testMatuDama, 1);

    return 0;
}