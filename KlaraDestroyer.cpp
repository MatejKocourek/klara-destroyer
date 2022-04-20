#include <iostream>
#include <map>

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



using namespace std;


class Board;

const int kingPrice = 1000000000;

bool itsTimeToStop;//atomic too slow
char depthW;
char onMoveW;



struct Piece {
    virtual wchar_t print() const = 0;/*
    {
        return L' ';
    }*/

    virtual vector<pair<Board, float>> availablePositions(const Board& board, char column, char row) const = 0;/*
    {
        return {};
    }*/
    /*
        virtual vector<pair<char,char>> availablePositionsNonBlockable(){
            return {};
        }
        virtual vector<pair<char,char>> availablePositionsTakeOnly()
        {
            return {};
        }
        virtual vector<pair<char,char>> availablePositionsBlockable()
        {
            return {};
        }*/

    virtual char occupancy() const = 0;/*
    {
        return 0;
    }*/

    virtual float price(char row) const = 0;/*
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

    virtual float bestPosition(Board& board, char column, char row, int_fast8_t depth, float& alpha, float& beta, uint_fast16_t& totalMoves, double& totalValues) = 0;/*
    {
        return 0;
    }*/

    virtual void tryChangeAndUpdateIfBetter(Board& board, char column, char row, int_fast8_t depth, float& alpha, float& beta, float& bestValue, double& totalValues, uint_fast16_t& totalMoves, bool& doNotContinue, Piece* changeInto = nullptr, float minPriceToTake = 0, float maxPriceToTake = FLT_MAX);

};


//const Piece emptyField;

class Board {
    Piece* pieces[64];
public:

    Board(const Board& copy)
    {
        for (int i = 0; i < 64; ++i) {
            if (copy.pieces[i] == nullptr)
                pieces[i] = nullptr;
            else
                pieces[i] = copy.pieces[i]->clone();
        }
    }
    Board(Board&& move)
    {
        for (int i = 0; i < 64; ++i) {
            if (move.pieces[i] == nullptr)
                pieces[i] = nullptr;
            else
            {
                pieces[i] = move.pieces[i];
                move.pieces[i] = nullptr;
            }

        }
    }

    Board& operator=(Board&& move)
    {
        for (int i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)
                delete pieces[i];
            pieces[i] = move.pieces[i];
            move.pieces[i] = nullptr;
        }
        return *this;
    }

    Board& operator=(const Board& copy)
    {
        for (int i = 0; i < 64; ++i) {
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
        for (int i = 0; i < 64; ++i) {
            pieces[i] = nullptr;//new Piece();
        }
    }

    ~Board()
    {
        for (int i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)
                delete pieces[i];
        }
    }




    float priceInLocation(char column, char row, char playerColor) const
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
    /*
        bool canStepInLocation(char column, char row, char playerColor){
            if(column<'a'||column>'h'||row<'1'||row>'8')
                return false;
            else
                return pieces[(column-'a')+(row-'1')*8]->occupancy() != playerColor;
        }*/
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
            return;
        else
        {
            delete pieces[(column - 'a') + (row - '1') * 8];
            pieces[(column - 'a') + (row - '1') * 8] = p;
        }
    }

    void deleteAndMovePiece(char columnFrom, char rowFrom, char columnTo, char rowTo)
    {
        const uint_fast8_t posFrom = (columnFrom - 'a') + (rowFrom - '1') * 8;
        const uint_fast8_t posTo = (columnTo - 'a') + (rowTo - '1') * 8;

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


    friend bool operator<(const Board& l, const Board& r)
    {
        return &l < &r;//prasarna
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
    float balance() const {
        float res = 0;
        for (int i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)
                res += pieces[i]->price(i / 8) * pieces[i]->occupancy();
        }
        //wcout<<res<<endl;
        //print();

        return res;
    }
    /*
    vector<pair<Board,float>> allPositionsScore(int_fast8_t depth, int_fast8_t onMove)
    {
        float alpha = -FLT_MAX;
        float beta = FLT_MAX;
        vector<pair<Board,float>> res;
        if (depth == 0)
            return res;
        double bestValue = INT32_MAX * onMove * (-1);

        uint_fast16_t totalMoves = 0;
        int depthToPieces = depth;

        for (int_fast8_t i = 0; i < 64; ++i) {
            Piece* found = pieces[i];

            if (found == nullptr)
                continue;
            if (found->occupancy() == onMove)
            {
                auto foundVal = found->bestPosition(*this, (i & 0b111) + 'a', (i >> 3) + '1', depthToPieces, alpha, beta, totalMoves);
            }

            //firstMoves.insert(firstMoves.end(),possibleMoves.begin(),possibleMoves.end());
        }


        //if(depth==1)
            //bestValue+=((totalMoves*onMove)/1000.0);


        if (depthW != depth)//sude hloubky hraje PC, liche souper. DepthW je depth na zacatku, to hraje souper, tah PC neni ohodnocen, takze by bylo nefer ohodnotit souperuv
        {
            //Vesti cislo depth znamena ze nejsem jiz tak hluboko (tahy nastanou drive)
            int_fast8_t howFarFromInitial = (depthW - depth);
            int_fast8_t howFarFromInitialAfterExchange = ((howFarFromInitial + 1) >> 1);//Kolikaty tah od initial pozice (od 0), ne pultah
            //bestValue -= onMove*(howFarFromInitialAfterExchange / 1000.0);//Pozdejsi tahy maji nizsi vahu, nevidim tam tolik dopredu co muze nasledovat - penalizovat hloubku a uprednostnovat co nejrychleji se dostat do vyhody, ale idealne aby se pouzilo v prioritě až nakonec, když je lepší tah tak si na něj počkám. Lepší počítat půltahy, přece jenom je to půtah informace navíc, může rozhodnout výměny např. dámy atd

            //Cil pokryt co nejvic poli, cim pozdeji tim lip. Napadena pole se pocitaji za vicenasobek, viz metoda u Piece
            //if(depth<=2)//Zajimaji me pouze finální pozice, než k nim dojdu může být pozice klidně špatná jak chce (to že si je méně jistý koriguje druhý odečet). Omezení na hloubku 2 taky velmi zrychli program kvuli aplha/beta prorezavani.
                //bestValue += onMove*(totalMoves / 100000.0);//Cim vice moves, tim vetsi vyhoda. Konstanta je zvolená odhadem, těžko říct jakou zvolit
            bestValue += onMove * (totalMoves / (1000.0 * (howFarFromInitialAfterExchange + 1)));
        }

        //wcout<<"moves"<<totalMoves<<endl;
        //print();


        //doneMoves.emplace(board.printBasic(),bestValue);
        return bestValue;//+((totalMoves*onMove)/(1000.0*depth));
    }*/

    float bestPositionScore(int_fast8_t depth, char onMove, float alpha = -FLT_MAX, float beta = FLT_MAX)
    {
        if (depth == 0)
            return 0;
        double bestValue = INT32_MAX * onMove * (-1);
        double totalValues = 0;
        //wcout << endl;
        //print();
        if (depth == depthW)
        {
            
            //wcout << endl;
            
        }

        

        if (itsTimeToStop)
            return 0;//mozna?

        //if(beta<=alpha)
          //  return INT32_MAX*onMove*(-1);//bestValue;


        uint_fast16_t totalMoves = 0;
        int depthToPieces = depth;

        for (int_fast8_t i = 0; i < 64; ++i) {
            Piece* found = pieces[i];

            if (found == nullptr)
                continue;
            if (found->occupancy() == onMove)
            {
                auto foundVal = found->bestPosition(*this, (i & 0b111) + 'a', (i >> 3) + '1', depthToPieces, alpha, beta, totalMoves, totalValues);

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
                /*if (beta <= alpha)
                {
                    depthToPieces = 0;
                    //break;
                }*/
            }
        }

        


        //if(beta<=alpha)
          //  return 0;//INT32_MAX*onMove*(-1);

        if (totalMoves == 0)//Nemůžu udělat žádný legitimní tah (pat nebo mat)
        {
            //print();
            if (bestPositionScore(1, onMove * (-1), alpha, beta) * onMove * (-1) == kingPrice)//Soupeř je v situaci, kdy mi může vzít krále (ohrožuje ho)
            {
                //print();
                bestValue = -100000 * depth * onMove;//Dostanu mat, co nejnižší skóre
            }

            else
            {//Dostal bych pat, ten je vždycky lepší než dostat mat, ale chci ho docílit jen když prohrávám
                if (balance() * onMove > 0)
                    bestValue = -500 * depth * onMove;//Vedu, nechci dostat remízu
                else
                    bestValue = 500 * depth * onMove;//Prohrávám nebo nastejno, dát remízu beru jako super tah
            }

        }


        //if(depth==1)
            //bestValue+=((totalMoves*onMove)/1000.0);

        
        if (depthW != depth)//sude hloubky hraje PC, liche souper. DepthW je depth na zacatku, to hraje souper, tah PC neni ohodnocen, takze by bylo nefer ohodnotit souperuv
        {
            double avgValue = totalValues / totalMoves;
            //if (avgValue != 0)
           // {
            //    int a = 4;
            //}
            avgValue /= 1000.0;
            avgValue = max(min(avgValue, 0.01),-0.01);
            bestValue += avgValue;

            //Vesti cislo depth znamena ze nejsem jiz tak hluboko (tahy nastanou drive)
            int_fast8_t howFarFromInitial = (depthW - depth);
            int_fast8_t howFarFromInitialAfterExchange = ((howFarFromInitial + 1) >> 1);//Kolikaty tah od initial pozice (od 0), ne pultah
            //bestValue -= onMove*(howFarFromInitialAfterExchange / 1000.0);//Pozdejsi tahy maji nizsi vahu, nevidim tam tolik dopredu co muze nasledovat - penalizovat hloubku a uprednostnovat co nejrychleji se dostat do vyhody, ale idealne aby se pouzilo v prioritě až nakonec, když je lepší tah tak si na něj počkám. Lepší počítat půltahy, přece jenom je to půtah informace navíc, může rozhodnout výměny např. dámy atd

            //Cil pokryt co nejvic poli, cim pozdeji tim lip. Napadena pole se pocitaji za vicenasobek, viz metoda u Piece
            if(depth<=2)//Zajimaji me pouze finální pozice, než k nim dojdu může být pozice klidně špatná jak chce (to že si je méně jistý koriguje druhý odečet). Omezení na hloubku 2 taky velmi zrychli program kvuli aplha/beta prorezavani.
                bestValue += onMove*(totalMoves / 100000.0);//Cim vice moves, tim vetsi vyhoda. Konstanta je zvolená odhadem, těžko říct jakou zvolit
            //bestValue += onMove * (totalMoves / (1000.0 * (howFarFromInitialAfterExchange + 1)));
        }

        //wcout<<"moves"<<totalMoves<<endl;
        //print();


        //doneMoves.emplace(board.printBasic(),bestValue);
        return bestValue;//+((totalMoves*onMove)/(1000.0*depth));
    }


    float tryPiece(char column, char row, Piece* p, char depth, char occupancyToTry, float alpha, float beta)
    {
        Piece* backup = pieceAt(column, row);
        setPieceAt(column, row, p);
        auto foundVal = bestPositionScore(depth, occupancyToTry, alpha, beta);
        setPieceAt(column, row, backup);
        return foundVal;
    }

    /*
        Piece whatPieceAtLocation(char column, char row){
            if(column<'a'||column>'h'||row<'1'||row>'8')
                return Piece();
        }*/

};


void Piece::tryChangeAndUpdateIfBetter(Board& board, char column, char row, int_fast8_t depth, float& alpha, float& beta, float& bestValue, double& totalValues, uint_fast16_t& totalMoves, bool& doNotContinue, Piece* changeInto, float minPriceToTake, float maxPriceToTake)
{
    if (doNotContinue)
        return;
    float price = board.priceInLocation(column, row, occupancy());
    if (price >= minPriceToTake && price <= maxPriceToTake)
    {


        //int test = totalMoves;
        totalMoves += 1;
        //if (doNotContinue)
          //  return;

        float foundVal = price * occupancy();



        if (price == kingPrice)//Je možné vzít krále
        {
            doNotContinue = true;
            //bestValue=price*occupancy();
            //totalMoves++;
            //return;
        }
        else if (depth >= 1)
        {
            if (changeInto == nullptr)
                changeInto = this;
            float foundOnly = board.tryPiece(column, row, changeInto, depth, occupancy() * (-1), alpha, beta);

            foundVal += foundOnly;
            
            if ((depth > 1 && (int)foundOnly * occupancy() * (-1) == kingPrice))//V dalším tahu bych přišel o krále, není to legitimní tah
            {
                //if (foundVal > 0)
                totalMoves -= 1;
                totalValues -= foundVal;
                //else
                  //  totalMoves-=1;
            }
            //else if (price > 0)
              //  totalMoves += 4;
        }

        totalValues += foundVal;

        if (foundVal * occupancy() > bestValue * occupancy())
            bestValue = foundVal;

        if (occupancy() > 0)
            alpha = max(alpha, bestValue);
        else
            beta = min(beta, bestValue);

        //if(totalMoves>0&&beta<=alpha)
          //  doNotContinue=true;
        if (beta <= alpha && totalMoves > 0)
        {
            //bestValue=INT32_MAX*occupancy()*(-1);
            doNotContinue = true;
        }

    }
}


struct Pawn : public Piece {
    //virtual bool canBeEvolved(char row) const = 0;
    virtual char evolveRow() const = 0;

    virtual vector<Piece*> evolveInto(char row) const = 0;

    virtual vector<Piece*>* evolveIntoReference(char row) const = 0;

    virtual char advanceRow() const = 0;

    virtual bool canGoTwoFields(char row) const = 0;


    virtual float bestPosition(Board& board, char column, char row, int_fast8_t depth, float& alpha, float& beta, uint_fast16_t& totalMoves, double& totalValues) override
    {
        float bestValue = INT32_MAX * (-1) * occupancy();

        //if (depth <= 0)
          //  return 0;

        auto availableOptions = evolveIntoReference(row + advanceRow());

        bool doNoContinue = false;

        //float rowAdvantage = 0;
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

                tryChangeAndUpdateIfBetter(board, column, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, (*availableOptions)[i], 0, 0);
                if (canGoTwoFields(row))
                    tryChangeAndUpdateIfBetter(board, column, row + advanceRow() * 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, (*availableOptions)[i], 0, 0);

            }
        }
        for (int_fast8_t i = 0; i < availableOptions->size(); ++i) {
            tryChangeAndUpdateIfBetter(board, column + 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, (*availableOptions)[i], FLT_MIN);
            tryChangeAndUpdateIfBetter(board, column - 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, (*availableOptions)[i], FLT_MIN);
        }
        board.setPieceAt(column, row, this);

        return bestValue;
    }

    vector<pair<Board, float>> availablePositions(const Board& board, char column, char row) const override
    {
        vector<pair<Board, float>> res;
        vector<Piece*> availableOptions = evolveInto(row + advanceRow());
        //float rowAdvantage = 0;
        //if(availableOptions.size()==1)
        //{
            //rowAdvantage = (6-abs(row-evolveRow()))/128.0;

            //bool allLineClear = true;
            //for (char i = row; i != evolveRow(); i+=advanceRow()) {
             //   if(board.priceInLocation(column,i,0)!=0)
              //  {
               //     allLineClear=false;

                //}
            //}
        //}


        if (board.priceInLocation(column, row + advanceRow(), occupancy()) == 0) {

            for (int_fast8_t i = 0; i < availableOptions.size(); ++i) {
                res.emplace_back(board, availableOptions[i]->price(row + advanceRow()) - price(row));//+rowAdvantage);

                res[res.size() - 1].first.deleteAndMovePiece(column, row, column, row + advanceRow());
                res[res.size() - 1].first.deleteAndOverwritePiece(column, row + advanceRow(), availableOptions[i]->clone());

            }

            if (canGoTwoFields(row) && board.priceInLocation(column, row + advanceRow() * 2, occupancy()) == 0) {
                res.emplace_back(board, price(row + advanceRow() * 2) - price(row));
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column, row + advanceRow() * 2);
            }

        }
        if (board.priceInLocation(column + 1, row + advanceRow(), occupancy()) > 0)
        {

            for (int_fast8_t i = 0; i < availableOptions.size(); ++i) {
                res.emplace_back(board, availableOptions[i]->price(row + advanceRow()) - price(row) + board.priceInLocation(column + 1, row + advanceRow(), occupancy()));//+rowAdvantage);

                res[res.size() - 1].first.deleteAndMovePiece(column, row, column + 1, row + advanceRow());
                res[res.size() - 1].first.deleteAndOverwritePiece(column + 1, row + advanceRow(), availableOptions[i]->clone());
            }
            //res.emplace_back(board,board.priceInLocation(column+1,row+1, occupancy()));
            //res[res.size()-1].first.movePiece(column,row,column+1,row+1);
        }
        if (board.priceInLocation(column - 1, row + advanceRow(), occupancy()) > 0)
        {
            for (int_fast8_t i = 0; i < availableOptions.size(); ++i) {
                res.emplace_back(board, availableOptions[i]->price(row + advanceRow()) - price(row) + board.priceInLocation(column - 1, row + advanceRow(), occupancy()));//+rowAdvantage);

                res[res.size() - 1].first.deleteAndMovePiece(column, row, column - 1, row + advanceRow());
                res[res.size() - 1].first.deleteAndOverwritePiece(column - 1, row + advanceRow(), availableOptions[i]->clone());
            }
            //res.emplace_back(board,board.priceInLocation(column-1,row+1, occupancy()));
            //res[res.size()-1].first.movePiece(column,row,column-1,row+1);
        }
        for (int i = 0; i < availableOptions.size(); ++i) {
            delete availableOptions[i];
        }

        return res;
    }
};


struct Knight : public Piece {

    virtual float bestPosition(Board& board, char column, char row, int_fast8_t depth, float& alpha, float& beta, uint_fast16_t& totalMoves,double& totalValues) override {

        float bestValue = INT32_MAX * (-1) * occupancy();

        //if (depth <= 0)
          //  return 0;
        board.setPieceAt(column, row, nullptr);

        bool foundKing = false;

        tryChangeAndUpdateIfBetter(board, column + 1, row + 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column + 1, row - 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column + 2, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column + 2, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column - 1, row + 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column - 1, row - 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column - 2, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column - 2, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);

        board.setPieceAt(column, row, this);

        return bestValue;
    }


    vector<pair<Board, float>> availablePositions(const Board& board, char column, char row) const override
    {
        vector<pair<Board, float>> res;


        if (board.priceInLocation(column + 1, row + 2, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column + 1, row + 2, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column + 1, row + 2);
        }
        if (board.priceInLocation(column + 1, row - 2, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column + 1, row - 2, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column + 1, row - 2);
        }
        if (board.priceInLocation(column + 2, row + 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column + 2, row + 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column + 2, row + 1);
        }
        if (board.priceInLocation(column + 2, row - 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column + 2, row - 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column + 2, row - 1);
        }
        if (board.priceInLocation(column - 1, row + 2, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column - 1, row + 2, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column - 1, row + 2);
        }
        if (board.priceInLocation(column - 1, row - 2, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column - 1, row - 2, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column - 1, row - 2);
        }
        if (board.priceInLocation(column - 2, row + 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column - 2, row + 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column - 2, row + 1);
        }
        if (board.priceInLocation(column - 2, row - 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column - 2, row - 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column - 2, row - 1);
        }


        return res;
    }


    virtual float price(char row) const override {
        return 3.2;
    }
};


struct Bishop : public Piece {
    virtual float bestPosition(Board& board, char column, char row, int_fast8_t depth, float& alpha, float& beta, uint_fast16_t& totalMoves, double& totalValues)  override {

        float bestValue = INT32_MAX * (-1) * occupancy();
        //if (depth <= 0)
          //  return 0;

        board.setPieceAt(column, row, nullptr);
        bool foundKing = false;

        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }


        board.setPieceAt(column, row, this);

        return bestValue;
    }




    vector<pair<Board, float>> availablePositions(const Board& board, char column, char row) const override
    {
        vector<pair<Board, float>> res;

        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row + i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column + i, row + i);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row - i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column + i, row - i);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row + i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column - i, row + i);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row - i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column - i, row - i);
                if (price > 0)
                    break;
            }
            else
                break;
        }


        return res;
    }


    virtual float price(char row) const override {
        return 3.33;
    }
};


struct Rook : public Piece {
    virtual float bestPosition(Board& board, char column, char row, int_fast8_t depth, float& alpha, float& beta, uint_fast16_t& totalMoves, double& totalValues) override {

        float bestValue = INT32_MAX * (-1) * occupancy();
        //if (depth <= 0)
          //  return 0;
        //int_fast8_t totalMoves = 0;

        board.setPieceAt(column, row, nullptr);

        bool foundKing = false;

        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }


        board.setPieceAt(column, row, this);

        return bestValue;
    }


    vector<pair<Board, float>> availablePositions(const Board& board, char column, char row) const override
    {
        vector<pair<Board, float>> res;

        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column + i, row);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column - i, row);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column, row + i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column, row + i);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column, row - i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column, row - i);
                if (price > 0)
                    break;
            }
            else
                break;
        }


        return res;
    }


    virtual float price(char row) const override {
        return 5.1;
    }
};


struct Queen : public Piece {

    virtual float bestPosition(Board& board, char column, char row, int_fast8_t depth, float& alpha, float& beta, uint_fast16_t& totalMoves, double& totalValues) override {
        //board.print();

        float bestValue = INT32_MAX * (-1) * occupancy();

        // if (depth <= 0)
          //   return 0;


        Piece* originalPiece = board.pieceAt(column, row);
        board.setPieceAt(column, row, nullptr);

        bool foundKing = false;


        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }

        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column, row + i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column, row + i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column, row - i, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column, row - i, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column + i, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }



        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row, occupancy());
            if (price >= 0)
            {
                tryChangeAndUpdateIfBetter(board, column - i, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
                if (price > 0)
                    break;
            }
            else
                break;
        }

        board.setPieceAt(column, row, this);

        return bestValue;
    }



    vector<pair<Board, float>> availablePositions(const Board& board, char column, char row) const override
    {
        vector<pair<Board, float>> res;


        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column + i, row);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column - i, row);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column, row + i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column, row + i);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column, row - i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column, row - i);
                if (price > 0)
                    break;
            }
            else
                break;
        }




        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row + i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column + i, row + i);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column + i, row - i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column + i, row - i);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row + i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column - i, row + i);
                if (price > 0)
                    break;
            }
            else
                break;
        }
        for (char i = 1; i < 8; ++i) {
            float price = board.priceInLocation(column - i, row - i, occupancy());
            if (price >= 0)
            {
                res.emplace_back(board, price);
                res[res.size() - 1].first.deleteAndMovePiece(column, row, column - i, row - i);
                if (price > 0)
                    break;
            }
            else
                break;
        }



        return res;
    }


    virtual float price(char row) const override {
        return 8.8;
    }
};

struct King : public Piece {


    virtual float bestPosition(Board& board, char column, char row, int_fast8_t depth, float& alpha, float& beta, uint_fast16_t& totalMoves, double& totalValues) override {
        float bestValue = INT32_MAX * (-1) * occupancy();
        //  if (depth <= 0)
          //    return 0;
        bool foundKing = false;

        board.setPieceAt(column, row, nullptr);

        tryChangeAndUpdateIfBetter(board, column + 1, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column + 1, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column + 1, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column - 1, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column - 1, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column - 1, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);
        tryChangeAndUpdateIfBetter(board, column, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, foundKing);

        board.setPieceAt(column, row, this);

        return bestValue;

    }



    vector<pair<Board, float>> availablePositions(const Board& board, char column, char row) const override
    {
        vector<pair<Board, float>> res;


        if (board.priceInLocation(column + 1, row + 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column + 1, row + 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column + 1, row + 1);
        }
        if (board.priceInLocation(column + 1, row, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column + 1, row, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column + 1, row);
        }
        if (board.priceInLocation(column + 1, row - 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column + 1, row - 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column + 1, row - 1);
        }
        if (board.priceInLocation(column, row - 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column, row - 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column, row - 1);
        }
        if (board.priceInLocation(column - 1, row - 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column - 1, row - 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column - 1, row - 1);
        }
        if (board.priceInLocation(column - 1, row, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column - 1, row, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column - 1, row);
        }
        if (board.priceInLocation(column - 1, row + 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column - 1, row + 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column - 1, row + 1);
        }
        if (board.priceInLocation(column, row + 1, occupancy()) >= 0) {
            res.emplace_back(board, board.priceInLocation(column, row + 1, occupancy()));
            res[res.size() - 1].first.deleteAndMovePiece(column, row, column, row + 1);
        }


        return res;
    }


    float price(char row) const override {
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

    vector<Piece*> evolveInto(char row) const override {
        vector<Piece*> res;
        if (row == evolveRow())
        {
            res.push_back(new QueenWhite());
            res.push_back(new RookWhite());
            res.push_back(new BishopWhite());
            res.push_back(new KnightWhite());
        }
        else
        {
            res.push_back(clone());
        }

        return res;
    }
    Piece* clone() const override {
        return new PawnWhite(*this);
    }
    char printBasic() const override {
        return 'p';
    }


    float price(char row) const override {
        return 1;
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

    vector<Piece*> evolveInto(char row) const override {
        vector<Piece*> res;
        if (row == evolveRow())
        {
            res.push_back(new QueenBlack());
            res.push_back(new RookBlack());
            res.push_back(new BishopBlack());
            res.push_back(new KnightBlack());
        }
        else
        {
            res.push_back(clone());
        }

        return res;
    }

    Piece* clone() const override {
        return new PawnBlack(*this);
    }
    char printBasic() const override {
        return 'P';
    }

    float price(char row) const override {
        return 1;
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


/*
struct BoardDerivative{
    BoardDerivative(Board board, Board * original, int price, int depth, char onMove):board(board),original(original),price(price),depth(depth),onMove(onMove){}

    Board board;
    Board* original;
    int price;
    int depth;
    char onMove;

    friend bool operator<(const BoardDerivative& l, const BoardDerivative& r)
    {
        return l.price-l.depth < r.price-r.depth;
    }
};*/

//map<string,float> doneMoves;
/*
double findBestOn(Board & board, int depth, char onMove)
{
    if(depth==0)
        return 0;


    double bestValue = INT32_MAX*(-1)*onMove;
    int totalMoves = 0;

    //vector<pair<Board,int>> firstMoves;
    for (char j = '1'; j <= '8'; ++j) {
        for (char i = 'a'; i <= 'h'; ++i) {
            Piece *found = board.pieceAt(i, j);
            if (found->occupancy() == onMove)
            {
                auto possibleMoves = found->availablePositions(board, i, j);
                totalMoves+=possibleMoves.size();


                for (int k = 0; k < possibleMoves.size(); ++k) {
                    if(possibleMoves[k].second==kingPrice)//Je možné vzít krále
                    {
                        return (possibleMoves[k].second*depth)*onMove;
                    }
                    float foundOnly = 0;
                    if(depth>1)
                    {
                        foundOnly = findBestOn(possibleMoves[k].first,depth-1,onMove*(-1));
                        if((int)foundOnly*onMove*(-1)==kingPrice*(depth-1))//V dalším tahu bych přišel o krále, není to legitimní tah
                        {
                            totalMoves--;
                        }
                    }

                    //float foundOnly = findBestOn(possibleMoves[k].first,depth-1,onMove*(-1));

                    //if(depth>1&&(int)foundOnly*onMove*(-1)==kingPrice*(depth-1))//V dalším tahu bych přišel o krále, není to legitimní tah
                    //{
                    //    totalMoves--;
                    //}

                    float foundVal = foundOnly+possibleMoves[k].second*onMove;
                    if (foundVal*onMove>bestValue*onMove)
                        bestValue=foundVal;
                }

                //firstMoves.insert(firstMoves.end(),possibleMoves.begin(),possibleMoves.end());
            }
        }

        //wcout<<depth<<endl;
    }
    if(totalMoves==0)//Nemůžu udělat žádný legitimní tah (pat nebo mat)
    {
        if(findBestOn(board,1,onMove*(-1))*onMove*(-1)==kingPrice)//Soupeř je v situaci, kdy mi může vzít krále (ohrožuje ho)
            return -1000*onMove*depth;//Dostanu mat, co nejnižší skóre
        else
        {//Dostal bych pat, ten je vždycky lepší než dostat mat, ale chci ho docílit jen když prohrávám
            if(board.balance()*onMove>0)//Vedu
                return -500*onMove;
            else
                return 500*onMove;//Prohrávám nebo nastejno
        }

        return 0;
    }


    //doneMoves.emplace(board.printBasic(),bestValue);
    return bestValue+(totalMoves*onMove)/(1000.0*depth);
}*/

atomic<float> alphaOrBeta;

class ToProcessQueue
{
    std::mutex m;
    std::queue<pair<double*, Board>> q;
    condition_variable cv_empty;
public:
    void push(pair<double*, Board>&& item)
    {
        unique_lock<mutex> ul(m);
        q.push(move(item));
        cv_empty.notify_one();
    }
    void push(pair<double*, Board>& item)
    {
        unique_lock<mutex> ul(m);
        q.push(item);
        cv_empty.notify_one();
    }
    pair<double*, Board> pop()
    {
        unique_lock<mutex> ul(m);
        cv_empty.wait(ul, [this]() { return (!q.empty()); });
        auto res = q.front();
        q.pop();
        return res;
    }

};

ToProcessQueue q;

void findOutArgument(Board board, int_fast8_t depth, char onMove, double* res)//, float alpha = -FLT_MAX, float beta = FLT_MAX)
{
    Board toDestroy(board);

    if (onMove < 0)
    {
        float result = toDestroy.bestPositionScore(depth, onMove, alphaOrBeta, FLT_MAX);
        alphaOrBeta = max(alphaOrBeta * 1.0, result * 1.0);
        *res += result;
    }
    else
    {
        float result = toDestroy.bestPositionScore(depth, onMove, -FLT_MAX, alphaOrBeta);
        alphaOrBeta = min(alphaOrBeta * 1.0, result * 1.0);
        *res += result;
    }


    wcout<<"New alpha/beta: "<<alphaOrBeta<<endl;
}


void workerFromQ()//, float alpha = -FLT_MAX, float beta = FLT_MAX)
{
    while (true)
    {
        auto tmp = q.pop();
        if (*tmp.first == -FLT_MAX)
            return;
        findOutArgument(tmp.second, depthW, onMoveW, tmp.first);
    }

}

pair<Board, float> findBestOnSameLevel(vector<pair<double, Board>>& boards, int_fast8_t depth, char onMove)
{
    const int_fast8_t threadCount = 1;//thread::hardware_concurrency(); //4;
    vector<thread> threads;



    if (onMove < 0)
        alphaOrBeta = -FLT_MAX;
    else
        alphaOrBeta = FLT_MAX;

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
            q.push({ &it->first,it->second });
        }
    }
    else
    {
        for (auto& board : boards) {
            q.push({ &board.first,board.second });
        }
    }




    double stopper = -FLT_MAX;

    for (int i = 0; i < threadCount; ++i) {
        q.push({ &stopper,Board() });
    }

    for (int i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    std::sort(boards.begin(), boards.end());

    
    for (int i = 0; i < boards.size(); ++i) {
        wcout << boards[i].first << endl;
        boards[i].second.print();
    }
    wcout << endl << endl;


    if (onMove == 1)
    {
        return { boards[0].second,boards[0].first };
    }
    else
    {
        return { boards[boards.size() - 1].second,boards[boards.size() - 1].first };
    }

    //b.print();
    //wcout<<depth<<endl;
    //return bestValue;
}

void timeLimit(int milliseconds)
{
    this_thread::sleep_for(chrono::milliseconds(milliseconds));
    itsTimeToStop = true;
}

vector<pair<double, Board>> allBoardsFromPosition(Board& board, char onMove)
{
    vector<pair<double, Board>> firstMoves;
    firstMoves.reserve(200);

    for (char j = '1'; j <= '8'; ++j) {
        for (char i = 'a'; i <= 'h'; ++i) {
            Piece* found = board.pieceAt(i, j);
            if (found == nullptr)
                continue;
            if (found->occupancy() == onMove)
            {
                auto possibleMoves = found->availablePositions(board, i, j);
                for (int k = 0; k < possibleMoves.size(); ++k) {
                    //if(possibleMoves[k].second)

                    firstMoves.emplace_back(possibleMoves[k].second * onMove, possibleMoves[k].first);

                    if (possibleMoves[k].second * onMove == kingPrice)//king
                    {
                        wcout << "Neplatny tah!" << endl;
                        return {};
                        //firstMoves[firstMoves.size()-1].first;
                    }

                }
            }
        }
    }

    return firstMoves;

}

pair<Board, float> findBestOnTopLevel(Board& board, int_fast8_t depth, char onMove)
{
    auto tmp = allBoardsFromPosition(board, onMove);
    return findBestOnSameLevel(move(tmp), depth - 1, onMove * (-1));


    /*
    vector<pair<double,Board>> firstMoves;
    firstMoves.reserve(200);

    for (char j = '1'; j <= '8'; ++j) {
        for (char i = 'a'; i <= 'h'; ++i) {
            Piece *found = board.pieceAt(i, j);
            if(found==nullptr)
                continue;
            if (found->occupancy() == onMove)
            {
                auto possibleMoves = found->availablePositions(board, i, j);
                for (int k = 0; k < possibleMoves.size(); ++k) {
                    //if(possibleMoves[k].second)

                    firstMoves.emplace_back(possibleMoves[k].second*onMove,possibleMoves[k].first);

                    if(possibleMoves[k].second*onMove==kingPrice)//king
                    {
                        firstMoves[firstMoves.size()-1].first*=depth;
                    }
                    //possibleMoves[k].first.print();
                    //firstMoves[firstMoves.size()-1].first += possibleMoves[k].first.bestPosition(depth,onMove*(-1));
                    //possibleMoves[k].first.print();



                    //double foundVal = findBestOn(possibleMoves[k].first,depth-1,onMove*(-1))+possibleMoves[k].second*onMove;





                    //double foundVal = findBestOn(possibleMoves[k].first,depth-1,onMove*(-1))+possibleMoves[k].second*onMove;
                    //pair<double,Board> f(foundVal,possibleMoves[k].first);

                    //firstMoves.insert(upper_bound(firstMoves.begin(),firstMoves.end(),f),move(f));


                }
            }
        }
    }

    */

}

pair<Board, float> findBestInTimeLimit(Board& board, char onMove, int milliseconds)
{
    auto limit = thread(timeLimit, milliseconds);

    auto boardList = allBoardsFromPosition(board, onMove);

    pair<Board, float> res;

    wcout << "Depth: ";
    for (int_fast8_t i = 4; i < 100; i += 2) {
        auto bestPosFound = findBestOnSameLevel(boardList, i, onMove * (-1));
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

pair<Board, float> findBestInNumberOfMoves(Board& board, char onMove, char moves)
{
    auto boardList = allBoardsFromPosition(board, onMove);

    pair<Board, float> res;

    wcout << "Depth: ";
    for (int_fast8_t i = 5; i < moves; i += 2) {
        auto bestPosFound = findBestOnSameLevel(boardList, i, onMove * (-1));
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

void playGameInTime(Board board, char onMove, int timeToPlay)
{
    board.print();
    while (true)
    {
        auto start = chrono::high_resolution_clock::now();
        //doneMoves.clear();
        //auto result = findBestOnTopLevel(board,depth,onMove);
        //auto result = findBestInTimeLimit(board, onMove, timeToPlay);
        auto result = findBestInNumberOfMoves(board, onMove, 8);
        board = result.first;
        wcout << "Best position found score change: " << result.second << endl;//<<"Total found score "<<result.second+result.first.balance()<<endl;
        auto end = chrono::high_resolution_clock::now();

        auto elapsed = chrono::duration_cast<chrono::milliseconds>(end - start).count() / 1000.0;
        wcout << "Done in " << elapsed << "s." << endl;

        board.print();
        string tmp;
        cin >> tmp;

        //char columnOrig,rowOrig,columnTo,rowTo;
        //cin>>columnOrig;
        //cin>>rowOrig;
        //cin>>columnTo;
        //cin>>rowTo;

        //initial.movePiece(columnOrig,rowOrig,columnTo,rowTo);

        board.deleteAndMovePiece(tmp[0], tmp[1], tmp[2], tmp[3]);
        if (tmp.size() > 4)
            board.deleteAndMovePiece(tmp[5], tmp[6], tmp[7], tmp[8]);


        board.print();

    }
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


    board.deleteAndMovePiece(tmp[0], tmp[1], tmp[2], tmp[3]);
    if (tmp.size() > 4)
        board.deleteAndMovePiece(tmp[5], tmp[6], tmp[7], tmp[8]);

    auto elapsedHuman = chrono::duration_cast<chrono::milliseconds>(endHuman - startHuman).count();
    wcout << "Human moved in " << elapsedHuman / 1000.0 << "s." << endl;

    board.print();
    return elapsedHuman + 10;
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
        //auto result = findBestOnTopLevel(board,depth,onMove);
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
    testMatu.deleteAndOverwritePiece('2', '8', new KingWhite());
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

    //klaraHra.bestPosition(6,1);
    //playGameResponding(startingPosition(), -1);
    //benchmark();
    //playGameResponding(startingPosition(), -1);
    benchmark(6, lossOfQueenPossible, -1);

    return 0;
}