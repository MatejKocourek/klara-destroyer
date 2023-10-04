#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <cfloat>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <array>

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
#include <random>
#include <cassert>


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

const int kingPrice = 1000;

bool itsTimeToStop;//atomic too slow
bool evaluateMoves = false;
char depthW;
char onMoveW;
bool deterministic;


struct Piece {
    virtual wchar_t print() const = 0;/*
    {
        return L' ';
    }*/

    virtual constexpr int_fast8_t occupancy() const = 0;/*
    {
        return 0;
    }*/

    virtual constexpr float price(uint_fast8_t column, uint_fast8_t row) const = 0;

    virtual constexpr float pricePiece() const = 0;

    virtual constexpr float priceAdjustment(uint_fast8_t column, uint_fast8_t row) const = 0;

    virtual Piece* clone() const = 0;/*
    {
        return new Piece(*this);
    }*/

    virtual char printBasic() const = 0;/*
    {
        return ' ';
    }*/

    virtual double bestMoveWithThisPieceScore(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNotContinue = false) = 0;/*
    {
        return 0;
    }*/

    virtual void tryChangeAndUpdateIfBetter(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, double& bestValue, double& totalValues, uint_fast16_t& totalMoves, bool& doNotContinue, double valueSoFar, Piece* changeInto = nullptr, double minPriceToTake = 0, double maxPriceToTake = DBL_MAX);

    virtual ~Piece() = default;
};

bool saveToVector = false;

//const Piece emptyField;

class Board {
    std::array<Piece*, 64> pieces;
    //Piece* pieces[64];
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
            delete pieces[i];
            pieces[i] = move.pieces[i];
            move.pieces[i] = nullptr;
        }
        return *this;
    }

    Board& operator=(const Board& copy)
    {
        for (uint_fast8_t i = 0; i < 64; ++i) {
            delete pieces[i];
            if (copy.pieces[i] != nullptr)
                pieces[i] = copy.pieces[i]->clone();
            else
                pieces[i] = nullptr;
        }
        return *this;
    }


    Board() : pieces{{nullptr}}
    {
        //for (auto& i : pieces) {
        //    i = nullptr;
        //}
    }

    ~Board()
    {
        for (const auto& i : pieces) {
            delete i;
        }
    }

    float priceInLocation(char column, char row, int_fast8_t playerColor) const
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            return -1;

        //int_fast8_t index = (column - 'a') + ((row - '1') * 8);
        int_fast8_t index = -489 + column + (row << 3);


        if (pieces[index] == nullptr)
            return 0;
        if (pieces[index]->occupancy() == playerColor)
            return -1;
        else
            return pieces[index]->price(column-'a',row-'1');
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

    void print(char onMove=1) const
    {
        if (onMove<0)
        {
            for (int i = 0; i < 8; ++i) {
                wcout << i + 1 << ' ';
                for (int j = 7; j >= 0; --j) {
                    wcout << '|';
                    if (pieces[j + i * 8] != nullptr)
                        wcout << pieces[j + i * 8]->print();
                    else
                        wcout << L' ';
                }
                wcout << '|' << endl;
            }
            wcout << "   h g f e d c b a";
        }
        else
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
        }
        wcout << endl;
    }

    
    string printBasic() const
    {
        string res;
        for (uint_fast8_t i = 0; i < 64; ++i) {
            res += pieces[i]->printBasic();
        }
        return res;
    }
    
    float balance() const {
        float res = 0;
        for (uint_fast8_t i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr && pieces[i]->pricePiece()!=kingPrice)
                res += pieces[i]->price(i % 8, i / 8) * pieces[i]->occupancy();
        }

        return res;
    }

    double positionScoreHeuristic() {
        double res = 0;
        for (int_fast8_t i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)
            {
                double alpha, beta;
                alpha = -DBL_MAX;
                beta = DBL_MAX;
                uint_fast16_t totalMoves = 0;
                double totalValues = 0;
                pieces[i]->bestMoveWithThisPieceScore(*this, (i % 8) + 'a', (i / 8) + '1', 1, alpha, beta, totalMoves, totalValues, 0,true);

                res += (((double)totalMoves)/2.0) * pieces[i]->occupancy();
                if(totalMoves>0)
                    res += min(max(totalValues,-100.0),100.0);
            }
        }
        return res / 10000.0;
    }


    double bestMoveScore(int_fast8_t depth, char onMove, double valueSoFar, double alpha, double beta)
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
                auto foundVal = found->bestMoveWithThisPieceScore(*this, (i & 0b111) + 'a', (i >> 3) + '1', depthToPieces, alpha, beta, totalMoves, totalValues, valueSoFar);

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
            if ((int)(bestMoveScore(1, onMove * (-1),valueSoFar, alpha, beta) * onMove * (-1)) == kingPrice)//Soupeř je v situaci, kdy mi může vzít krále (ohrožuje ho)
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
        auto foundVal = bestMoveScore(depth, occupancyToTry, valueSoFar, alpha, beta);
        setPieceAt(column, row, backup);
        return foundVal;
    }

    double tryPiecePosition(char column, char row, Piece* p)
    {
        Piece* backup = pieceAt(column, row);
        setPieceAt(column, row, p);
        auto foundVal = positionScoreHeuristic();
        setPieceAt(column, row, backup);
        return foundVal;
    }

};


struct BoardWithValues {
    Board board;
    double bestFoundValue;
    double startingValue;

    BoardWithValues(const BoardWithValues& copy) = default;//:board(copy.board),bestFoundValue(copy.bestFoundValue),pieceTakenValue(copy.pieceTakenValue){}
    BoardWithValues(BoardWithValues&& toMove) noexcept = default;// :board(move(toMove.board)), bestFoundValue(toMove.bestFoundValue), pieceTakenValue(toMove.pieceTakenValue) {}


    //BoardWithValues& operator=(BoardWithValues&& toMove) = default;

    BoardWithValues& operator=(const BoardWithValues& copy) = default;

    friend bool operator<(const BoardWithValues& l, const BoardWithValues& r)
    {
        return l.bestFoundValue < r.bestFoundValue;
    }

    //BoardWithValues(Board board, double bestFoundValue, double startingValue):board(move(board)),bestFoundValue(bestFoundValue), startingValue(startingValue) {}
    BoardWithValues(Board board, double startingValue) :board(move(board)), bestFoundValue(startingValue), startingValue(startingValue) {}
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

        if (saveToVector)
        {
            if (changeInto == nullptr)
                changeInto = this;
            Piece* backup = board.pieceAt(column, row);
            board.setPieceAt(column, row, changeInto);

            double pieceTakenValue = valueSoFar + price * occupancy();
            firstPositions.emplace_back(board, board.balance());
            //firstPositions[firstPositions.size() - 1].board.print();
            board.setPieceAt(column, row, backup);
            //firstPositions[firstPositions.size() - 1].board.print();
            //wcout << endl;
            return;
        }

        if (doNotContinue)
            return;

        if (price >= kingPrice-50)//Je možné vzít krále
        {
            doNotContinue = true;
            bestValue = price * occupancy();
            //totalMoves++;
            //return;

        }
        else
        {
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
                if (evaluateMoves)
                    foundVal += board.tryPiecePosition(column, row, changeInto);
            }


            if (foundVal * occupancy() > bestValue * occupancy())
                bestValue = foundVal;
        }

        
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


struct Pawn : virtual public Piece {
    //virtual bool canBeEvolved(char row) const = 0;
    virtual char evolveRow() const = 0;

    virtual vector<Piece*>* evolveIntoReference(char row) const = 0;

    virtual char advanceRow() const = 0;

    virtual bool canGoTwoFields(char row) const = 0;

    //virtual double priceRelative(int_fast8_t relativeRowDistanceFromStart) const {

    //    if (relativeRowDistanceFromStart >= 5)
    //        return 1.000001;
    //    return 1;
    //    //return 1;
    //    //switch (relativeRowDistanceFromStart) {
    //    ////case 4:
    //    //  //  return 1.1;
    //    //case 5:
    //    //    return 1.25;
    //    //case 6:
    //    //    return 2;
    //    //case 7:
    //    //    return 3;
    //    //default:
    //    //    return 1;

    //    //}
    //}

    constexpr float pricePiece() const final
    {
        return 10;
    }
    

    constexpr float priceAdjustment(uint_fast8_t column, uint_fast8_t row) const final
    {
        assert(column < 8 && row < 8);
        constexpr std::array<std::array<float, 8>, 8> arr = { {
            {+0.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0},
            {+5.0, +5.0, +5.0, +5.0, +5.0, +5.0, +5.0, +5.0},
            {+1.0, +1.0, +2.0, +3.0, +3.0, +2.0, +1.0, +1.0},
            {+0.5, +0.5, +1.0, +2.5, +2.5, +1.0, +0.5, +0.5},
            {+0.0, +0.0, +0.0, +2.0, +2.0, +0.0, +0.0, +0.0},
            {+0.5, -0.5, -1.0, +0.0, +0.0, -1.0, -0.5, +0.5},
            {+0.5, +1.0, +1.0, -2.0, -2.0, +1.0, +1.0, +0.5},
            {+0.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0},
        } };

        return arr[row][column];
    }

    virtual double bestMoveWithThisPieceScore(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override
    {
        double bestValue = INT32_MAX * (-1) * occupancy();

        uint_fast16_t moves = 0;

        auto availableOptions = evolveIntoReference(row + advanceRow());


        board.setPieceAt(column, row, nullptr);

        if (board.priceInLocation(column, row + advanceRow(), occupancy()) == 0)//Muzu jit pescem o jedno dopredu
        {

            for (int_fast8_t i = 0; i < availableOptions->size(); ++i) {
                //One field forward
                {
                    double valueDifferenceNextMove = ((*availableOptions)[i]->price(column - 'a', row - '1' + advanceRow()) - price(column - 'a', row - '1')) * occupancy();
                    tryChangeAndUpdateIfBetter(board, column, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar + valueDifferenceNextMove, (*availableOptions)[i], 0, 0);
                }
                if (canGoTwoFields(row))//Two fields forward
                {
                    double valueDifferenceNextMove = ((*availableOptions)[i]->price(column - 'a', row - '1' + advanceRow() * 2) - price(column - 'a', row - '1')) * occupancy();
                    tryChangeAndUpdateIfBetter(board, column, row + advanceRow() * 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar + valueDifferenceNextMove, (*availableOptions)[i], 0, 0);
                }
                    

            }
        }
        for (int_fast8_t i = 0; i < availableOptions->size(); ++i) {
            if(column<'h')
            {
                double valueDifferenceNextMove = ((*availableOptions)[i]->price(column - 'a' + 1, row + advanceRow() - '1') - price(column - 'a', row - '1')) * occupancy();
                tryChangeAndUpdateIfBetter(board, column + 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar + valueDifferenceNextMove, (*availableOptions)[i], DBL_MIN);
            }
            if(column>'a')
            {
                double valueDifferenceNextMove = ((*availableOptions)[i]->price(column - 'a' - 1, row + advanceRow() - '1') - price(column - 'a', row - '1')) * occupancy();
                tryChangeAndUpdateIfBetter(board, column - 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar + valueDifferenceNextMove, (*availableOptions)[i], DBL_MIN);
            }
        }
        board.setPieceAt(column, row, this);


        return bestValue;
    }

};


struct Knight : virtual public Piece {

    virtual double bestMoveWithThisPieceScore(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {

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


    virtual constexpr float pricePiece() const final {
        return 32;
    }

    constexpr float priceAdjustment(uint_fast8_t column, uint_fast8_t row) const final
    {
        assert(column < 8 && row < 8);
        constexpr std::array<std::array<float, 8>, 8> arr = { {
            {-5.0, -4.0, -3.0, -3.0, -3.0, -3.0, -4.0, -5.0},
            {-4.0, -2.0, +0.0, +0.0, +0.0, +0.0, -2.0, -4.0},
            {-3.0, +0.0, +1.0, +1.5, +1.5, +1.0, +0.0, -3.0},
            {-3.0, +0.5, +1.5, +2.0, +2.0, +1.5, +0.5, -3.0},
            {-3.0, +0.0, +1.5, +2.0, +2.0, +1.5, +0.0, -3.0},
            {-3.0, +0.5, +1.0, +1.5, +1.5, +1.0, +0.5, -3.0},
            {-4.0, -2.0, +0.0, +0.5, +0.5, +0.0, -2.0, -4.0},
            {-5.0, -4.0, -3.0, -3.0, -3.0, -3.0, -4.0, -5.0},
        } };

        return arr[row][column];
    }
};


struct Bishop : virtual public Piece {
    virtual double bestMoveWithThisPieceScore(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue)  override {

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

    virtual constexpr float pricePiece() const final {
        return 33.3;
    }

    constexpr float priceAdjustment(uint_fast8_t column, uint_fast8_t row) const final
    {
        assert(column < 8 && row < 8);
        constexpr std::array<std::array<float, 8>, 8> arr = { {
            {-2.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -2.0},
            {-1.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, -1.0},
            {-1.0, +0.0, +0.5, +1.0, +1.0, +0.5, +0.0, -1.0},
            {-1.0, +0.5, +0.5, +1.0, +1.0, +0.5, +0.5, -1.0},
            {-1.0, +0.0, +1.0, +1.0, +1.0, +1.0, +0.0, -1.0},
            {-1.0, +1.0, +1.0, +1.0, +1.0, +1.0, +1.0, -1.0},
            {-1.0, +0.5, +0.0, +0.0, +0.0, +0.0, +0.5, -1.0},
            {-2.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -2.0},
        } };

        return arr[row][column];
    }
};


struct Rook : virtual public Piece {
    virtual double bestMoveWithThisPieceScore(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {

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

    virtual constexpr float pricePiece() const final {
        return 51;
    }

    constexpr float priceAdjustment(uint_fast8_t column, uint_fast8_t row) const final
    {
        assert(column < 8 && row < 8);
        constexpr std::array<std::array<float, 8>, 8> arr = { {
            {+0.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0},
            {+0.5, +1.0, +1.0, +1.0, +1.0, +1.0, +1.0, +0.5},
            {-0.5, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, -0.5},
            {-0.5, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, -0.5},
            {-0.5, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, -0.5},
            {-0.5, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, -0.5},
            {-0.5, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, -0.5},
            {+0.0, +0.0, +0.0, +0.5, +0.5, +0.0, +0.0, +0.0},
        } };

        return arr[row][column];
    }
};


struct Queen : virtual public Piece {

    virtual double bestMoveWithThisPieceScore(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {
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


    virtual constexpr float pricePiece() const final {
        return 88;
    }

    constexpr float priceAdjustment(uint_fast8_t column, uint_fast8_t row) const final
    {
        assert(column < 8 && row < 8);
        constexpr std::array<std::array<float, 8>, 8> arr = { {
            {-2.0, -1.0, -1.0, -0.5, -0.5, -1.0, -1.0, -2.0},
            {-1.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.0, -1.0},
            {-1.0, +0.0, +0.5, +0.5, +0.5, +0.5, +0.0, -1.0},
            {-0.5, +0.0, +0.5, +0.5, +0.5, +0.5, +0.0, -0.5},
            {+0.0, +0.0, +0.5, +0.5, +0.5, +0.5, +0.0, -0.5},
            {-1.0, +0.5, +0.5, +0.5, +0.5, +0.5, +0.0, -1.0},
            {-1.0, +0.0, +0.5, +0.0, +0.0, +0.0, +0.0, -1.0},
            {-2.0, -1.0, -1.0, -0.5, -0.5, -1.0, -1.0, -2.0},
        } };

        return arr[row][column];
    }
};

struct King : virtual public Piece {
    virtual double bestMoveWithThisPieceScore(Board& board, char column, char row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {
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

    virtual constexpr float pricePiece() const final {
        return kingPrice;
    }

    //double price(char column, char row) const override {

    //    return kingPrice;
    //}

    constexpr float priceAdjustment(uint_fast8_t column, uint_fast8_t row) const final
    {
        assert(column < 8 && row < 8);
        constexpr std::array<std::array<float, 8>, 8> arr = {{
            {-3.0, -4.0, -4.0, -5.0, -5.0, -4.0, -4.0, -3.0},
            {-3.0, -4.0, -4.0, -5.0, -5.0, -4.0, -4.0, -3.0},
            {-3.0, -4.0, -4.0, -5.0, -5.0, -4.0, -4.0, -3.0},
            {-3.0, -4.0, -4.0, -5.0, -5.0, -4.0, -4.0, -3.0},
            {-2.0, -3.0, -3.0, -4.0, -4.0, -3.0, -3.0, -2.0},
            {-1.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -1.0},
            {+2.0, +2.0, +0.0, +0.0, +0.0, +0.0, +2.0, +2.0},
            {+2.0, +3.0, +1.0, +0.0, +0.0, +1.0, +3.0, +2.0},
        }};

        //constexpr float tmp = arr[7][1];
        
        return arr[row][column];
    }
};

struct WhitePiece : virtual Piece {
    constexpr int_fast8_t occupancy() const final {
        return 1;
    }
    constexpr float price(uint_fast8_t column, uint_fast8_t row) const final {
        return pricePiece() + priceAdjustment(column, 7-row);
    }
};
struct BlackPiece : virtual Piece {
    constexpr int_fast8_t occupancy() const final {
        return -1;
    }
    constexpr float price(uint_fast8_t column, uint_fast8_t row) const final {
        return pricePiece() + priceAdjustment(7-column, row);
    }
};

struct KnightWhite final :public Knight, public WhitePiece {
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

struct KnightBlack :public Knight, public BlackPiece {
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

struct BishopWhite :public Bishop, public WhitePiece {
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

struct BishopBlack :public Bishop, public BlackPiece {
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

struct RookWhite :public Rook, public WhitePiece {
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

struct RookBlack :public Rook, public BlackPiece {
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
struct QueenWhite :public Queen, public WhitePiece {
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
struct QueenBlack :public Queen, public BlackPiece {
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
struct KingWhite :public King, public WhitePiece {
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
struct KingBlack :public King, public BlackPiece {
    //char occupancy() const override {
    //    return -1;
    //}
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


struct PawnWhite :public Pawn, public WhitePiece {
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


    //double price(char row) const override {
    //    return priceRelative(row - '0');
    //    ////return 1;
    //    //switch (row) {
    //    //case '5':
    //    //    return 1.5;
    //    //case '6':
    //    //    return 2;
    //    //case '7':
    //    //    return 3;
    //    //default:
    //    //    return 1;

    //    //}
    //}
    virtual vector<Piece*>* evolveIntoReference(char row) const;

};
struct PawnBlack :public Pawn, public BlackPiece {
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

    //double price(char row) const override {
    //    return priceRelative('9'- row);
    //    ////return 1;
    //    //switch (row) {
    //    //case '4':
    //    //    return 1.5;
    //    //case '3':
    //    //    return 2;
    //    //case '2':
    //    //    return 3;
    //    //default:
    //    //    return 1;

    //    //}
    //}

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
        board->bestFoundValue= toDestroy.bestMoveScore(depth, onMove, board->startingValue, alphaOrBeta, DBL_MAX);
        alphaOrBeta = max(alphaOrBeta * 1.0, board->bestFoundValue * 1.0);
    }
    else
    {
        board->bestFoundValue = toDestroy.bestMoveScore(depth, onMove,board->startingValue, -DBL_MAX, alphaOrBeta);
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

auto stopper = BoardWithValues(Board(), -DBL_MAX);



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

    //if (!itsTimeToStop)
    //{
    //    for (int i = 0; i < boards.size(); ++i) {
    //        wcout << boards[i].bestFoundValue << endl;
    //        wcout << boards[i].pieceTakenValue << endl;
    //        boards[i].board.print();
    //    }
    //    wcout << endl << endl;
    //}
    
    if (onMove == 1)
    {
        return { boards[0].board,boards[0].bestFoundValue/10 };
    }
    else
    {
        return { boards[boards.size() - 1].board,boards[boards.size() - 1].bestFoundValue/10 };
    }

}


void timeLimit(int milliseconds, bool * doNotStop)
{
    this_thread::sleep_for(chrono::milliseconds(milliseconds));
    if (!*doNotStop)
        itsTimeToStop = true;
    delete doNotStop;
    
}

vector<BoardWithValues> allBoardsFromPosition(Board& board, char onMove)
{
    itsTimeToStop = false;
    onMoveW = onMove;
    depthW = 1;
    saveToVector = true;
    board.bestMoveScore(1, onMove, 0, -DBL_MAX, DBL_MAX);
    saveToVector = false;
    auto res = firstPositions;
    firstPositions.clear();

    if (!deterministic)
    {
        std::random_device rd;
        auto rng = std::default_random_engine{ rd() };
        std::shuffle(res.begin(), res.end(), rng);
    }
    
    std::sort(res.begin(), res.end());

    //for (size_t i = 0; i < res.size(); i++)
    //{
    //    wcout << res[i].bestFoundValue << endl;
    //    res[i].board.print();
    //}

    return res;
}

pair<Board, double> findBestOnTopLevel(Board& board, int_fast8_t depth, char onMove)
{
    auto tmp = allBoardsFromPosition(board, onMove);
    return findBestOnSameLevel(tmp, depth - 1, onMove * (-1));
}

pair<Board, double> findBestInTimeLimit(Board& board, char onMove, int milliseconds, bool endSooner = true)
{
    evaluateMoves = false;

    bool* tellThemToStop = new bool;
    *tellThemToStop = false;

    auto limit = thread(timeLimit, milliseconds,tellThemToStop);
    auto start = chrono::high_resolution_clock::now();

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
        {
            limit.join();
            break;
        }

        res = bestPosFound;
        wcout << i + 1 << ' ';

        if (endSooner)
        {
            double elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start).count();
            double remaining = milliseconds - elapsed;

            if (remaining/4.0  < elapsed)
            {
                wcout << endl << "Would not likely finish next stage, optimizing out " << remaining / 1000 << "s.";
                itsTimeToStop = true;
                *tellThemToStop = true;
                limit.detach();//prasarna
                break;
            }
        }

        
    }
    wcout << endl;

    return res;
}

pair<Board, double> findBestInNumberOfMoves(Board& board, char onMove, char moves)
{
    evaluateMoves = false;
    auto boardList = allBoardsFromPosition(board, onMove);

    for (const auto& i : boardList)
    {
        std::wcout << i.bestFoundValue << std::endl;
        i.board.print();
        std::wcout << std::endl;
    }


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


long long boardUserInput(Board& board, char printToSide)
{

    string tmp;
    auto startHuman = chrono::high_resolution_clock::now();
    cin >> tmp;
    auto endHuman = chrono::high_resolution_clock::now();


    while (true)
    {
        try
        {
            if ((tmp.length()-4)%5!=0)
                throw exception("bad input");

            for (size_t i = 0; i < tmp.length(); i+=5)
            {
                if (tmp[0+i] == 'w' && tmp[1+i] == 'q')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], new QueenWhite());
                else if (tmp[0 + i] == 'b' && tmp[1 + i] == 'q')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], new QueenBlack());
                else if (tmp[0 + i] == 'w' && tmp[1 + i] == 'r')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], new RookWhite());
                else if (tmp[0 + i] == 'b' && tmp[1 + i] == 'r')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], new RookBlack());
                else if (tmp[0 + i] == 'w' && tmp[1 + i] == 'b')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], new BishopWhite());
                else if (tmp[0 + i] == 'b' && tmp[1 + i] == 'b')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], new BishopBlack());
                else if (tmp[0 + i] == 'w' && tmp[1 + i] == 'k')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], new KnightWhite());
                else if (tmp[0 + i] == 'b' && tmp[1 + i] == 'k')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], new KnightBlack());
                else
                {
                    board.deleteAndMovePiece(tmp[0 + i], tmp[1 + i], tmp[2 + i], tmp[3 + i]);
                    if (tmp.size() < 5 + i || tmp[4 + i] != '+')
                        break;
                }
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

    board.print(printToSide);
    return elapsedHuman + 10;
}


void playGameInTime(Board board, char onMove, int timeToPlay)
{
    board.print(onMove);
    if (onMove < 0)
    {
        boardUserInput(board,onMove);
    }
    
    while (true)
    {
        auto start = chrono::high_resolution_clock::now();
        auto balanceBefore = board.balance();
        auto result = findBestInTimeLimit(board, onMove, timeToPlay);
        board = result.first;
        wcout << "Score change: " << result.second << endl<<"Score result: "<< result.second+balanceBefore <<endl;
        auto end = chrono::high_resolution_clock::now();

        auto elapsed = chrono::duration_cast<chrono::milliseconds>(end - start).count() / 1000.0;
        wcout << "Done in " << elapsed << "s." << endl;
        board.print(onMove);

        while (isspace(cin.peek()))
            cin.get();

        char op = cin.get();

        if (op == '+' || op == '-' || op == '=')
        {
            int seconds;
            cin >> seconds;
            switch (op)
            {
            case('+'):
                timeToPlay += seconds * 1000;
                break;
            case('-'):
                timeToPlay -= seconds * 1000;
                break;
            case('='):
                timeToPlay = seconds * 1000;
                break;
            }
            wcout << "Time for move changed to " << timeToPlay << "ms." << endl;
        }
        else
        {
            cin.putback(op);
        }

        
        boardUserInput(board, onMove);

    }
}



void playGameResponding(Board board, char onMove)
{
    long long milliseconds = 5000;
    if (onMove < 0)
    {
        milliseconds = boardUserInput(board, onMove);
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

        board.print();
        milliseconds = boardUserInput(board, onMove);
    }
}


int main() {
    std::wcout.sync_with_stdio(false);
    std::wcout.precision(2);
    init_locale();
    deterministic = false;

    //benchmark(8);
    //
    // return 0;
    wcout << "White/Black? [w/b]" << endl;
    string color;
    cin >> color;

    char side = color[0] == 'w' ? 1 : -1;


    playGameInTime(startingPosition(), side, 10000);

    //Board promotion;
    //promotion.deleteAndOverwritePiece('h', '5', new PawnBlack);
    //promotion.deleteAndOverwritePiece('d', '5', new PawnBlack);
    //promotion.deleteAndOverwritePiece('f', '5', new KingBlack);
    //promotion.deleteAndOverwritePiece('g', '1', new BishopBlack);
    //promotion.deleteAndOverwritePiece('e', '2', new KingWhite);
    //promotion.deleteAndOverwritePiece('b', '5', new PawnWhite);
    //promotion.deleteAndOverwritePiece('a', '6', new PawnWhite);

    //promotion.print();

    //playGameResponding(promotion, 1);

    
    //playGameResponding(startingPosition(), -1);
    //benchmark(8);

    return 0;
}