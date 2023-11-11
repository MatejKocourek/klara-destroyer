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
#include "static_vector.h"
#include <array>
#include <string>
#include <syncstream>
#include <fstream>
#include <random>
#include <cassert>
#include <unordered_map>
#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <cfloat>
#include <atomic>
#include <mutex>
#include <condition_variable>

#define out std::cout
#define nl '\n'



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

using i8 = int_fast8_t;
using u8 = uint_fast8_t;
using i16 = int_fast16_t;
using u16 = uint_fast16_t;
using i32 = int_fast32_t;
using u32 = uint_fast32_t;
using i64 = int_fast64_t;
using u64 = uint_fast64_t;


class Board;

static constexpr double kingPrice = 2000;
static constexpr double matePrice = 1000000;
static constexpr double stalePrice = 5000;

enum PlayerSide : int_fast8_t
{
    WHITE = 1,
    BLACK = -1,
    //NONE = 0
};


static bool itsTimeToStop;//atomic too slow
static constexpr bool dynamicPositionRanking = false;//Not needed, static ranking is faster and should be enough
static int_fast8_t depthW;
static PlayerSide onMoveW;
static bool deterministic;
static std::chrono::steady_clock::time_point timeGlobalStarted;
static std::chrono::steady_clock::time_point timeDepthStarted;


struct Options {
    size_t MultiPV = 1;
} options;

constexpr inline PlayerSide oppositeSide(PlayerSide side) noexcept
{
    //[[assume(side==1||side==-1)]];
    return (PlayerSide)((-1) * (int_fast8_t)side);
}

struct Piece {
    virtual constexpr wchar_t symbolW() const = 0;

    virtual constexpr PlayerSide occupancy() const = 0;/*
    {
        return 0;
    }*/

    virtual constexpr float price(int_fast8_t column, int_fast8_t row) const = 0;

    virtual constexpr float pricePiece() const = 0;

    virtual constexpr float priceAdjustment(int_fast8_t column, int_fast8_t row) const = 0;

    //virtual Piece* clone() const = 0;/*

    virtual constexpr char symbolA() const = 0;

    std::ostream& print(std::ostream& os) const {
        return os << symbolA();
    }
    std::wostream& print(std::wostream& os) const {
        return os << symbolW();
    }

    virtual double bestMoveWithThisPieceScore(Board& board, int_fast8_t column, int_fast8_t row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNotContinue = false) = 0;/*
    {
        return 0;
    }*/

    void tryChangeAndUpdateIfBetter(Board& board, int_fast8_t column, int_fast8_t row, int_fast8_t depth, double& alpha, double& beta, double& bestValue, double& totalValues, uint_fast16_t& totalMoves, bool& doNotContinue, double valueSoFar, Piece* changeInto = nullptr, double minPriceToTake = 0, double maxPriceToTake = DBL_MAX);

    virtual ~Piece() = default;
};


struct BoardHasher
{
    std::size_t operator()(const Board& s) const noexcept;
};

static bool saveToVector = false;
//std::unordered_map<Board, float, BoardHasher> transpositions;


//const Piece emptyField;
class Board {
    //Piece* pieces[64];
public:
    std::array<Piece*, 64> pieces;
    PlayerSide playerOnMove;

    auto operator<=>(const Board&) const noexcept = default;


    Board(const Board& copy) = default;
    //{
    //    for (uint_fast8_t i = 0; i < 64; ++i) {
    //        if (copy.pieces[i] == nullptr)
    //            pieces[i] = nullptr;
    //        else
    //            pieces[i] = copy.pieces[i]->clone();
    //    }
    //}
    //Board(Board&& move) noexcept
    //{
    //    for (uint_fast8_t i = 0; i < 64; ++i) {
    //        pieces[i] = move.pieces[i];
    //        move.pieces[i] = nullptr;
    //    }
    //}

    //Board& operator=(Board&& move)
    //{
    //    for (uint_fast8_t i = 0; i < 64; ++i) {
    //        delete pieces[i];
    //        pieces[i] = move.pieces[i];
    //        move.pieces[i] = nullptr;
    //    }
    //    playerOnMove = move.playerOnMove;
    //    return *this;
    //}

    Board& operator=(const Board& copy) = default;
    //{
    //    for (uint_fast8_t i = 0; i < 64; ++i) {
    //        delete pieces[i];
    //        if (copy.pieces[i] != nullptr)
    //            pieces[i] = copy.pieces[i]->clone();
    //        else
    //            pieces[i] = nullptr;
    //    }
    //    playerOnMove = copy.playerOnMove;
    //    return *this;
    //}


    Board() : pieces{ {nullptr} }
    {
        //for (auto& i : pieces) {
        //    i = nullptr;
        //}
    }

    ~Board() = default;
    //{
    //    for (const auto& i : pieces) {
    //        delete i;
    //    }
    //}

    std::array<int_fast8_t, 128> piecesCount() const
    {
        std::array<int_fast8_t, 128> res { 0 };
        for (const auto& i : pieces)
        {
            if (i != nullptr)
                ++res[i->symbolA()];
        }
        return res;
    }

    bool canTakeKing(PlayerSide onMove)
    {
        uint_fast16_t totalMoves = 0;
        double totalValues = 0;
        constexpr double valueSoFar = 0;

        double alpha, beta;
        alpha = -DBL_MAX;
        beta = DBL_MAX;

        auto backup = playerOnMove;
        playerOnMove = onMove;

        for (int_fast8_t i = 0; i < pieces.size(); ++i) {
            Piece* found = pieces[i];

            if (found == nullptr)
                continue;
            if (found->occupancy() == onMove)
            {
                auto foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, totalMoves, totalValues, valueSoFar);

                if (foundVal * onMove == kingPrice) [[unlikely]]//Je možné vzít krále, hra skončila
                {
                    playerOnMove = backup;
                    return true;
                }
                    
            }
        }


        playerOnMove = backup;
        return false;
    }

    bool canMove(PlayerSide onMove)
    {
        uint_fast16_t totalMoves = 0;
        double totalValues = 0;
        constexpr double valueSoFar = 0;

        double alpha, beta;
        alpha = -DBL_MAX;
        beta = DBL_MAX;

        for (int_fast8_t i = 0; i < pieces.size(); ++i) {
            Piece* found = pieces[i];

            if (found == nullptr)
                continue;
            if (found->occupancy() == onMove)
            {
                auto foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, totalMoves, totalValues, valueSoFar);

                if (foundVal != INT16_MAX * (-1) * onMove)
                    return true;
            }
        }
        return false;
    }

    bool isValidSetup()
    {
        //bool backup = saveToVector;
        //saveToVector = false;
        //Both kings must be on the researchedBoard exactly once
        auto count = piecesCount();
        if (count['k'] != 1 || count['K'] != 1)
            return false;

        //print(std::cerr);
        
        //if (!canMove(opposideSide(playerOnMove)))
          //  return false;
        if (canTakeKing(playerOnMove))
            return false;

        //saveToVector = backup;

        //TODO the rest
        return true;
    }

    float priceInLocation(int_fast8_t column, int_fast8_t row, PlayerSide playerColor) const
    {
        if (column < 0 || column > 7 || row < 0 || row > 7)
            return -1;

        //int_fast8_t index = (column - 'a') + ((row - '1') * 8);
        const auto& piece = pieces[column + (row * 8)];


        if (piece == nullptr)
            return 0;
        if (piece->occupancy() == playerColor)
            return -1;
        else
            return piece->price(column,row);
    }

    Piece* pieceAt(int_fast8_t column, int_fast8_t row)
    {
        assert (!(column < 0 || column > 7 || row < 0 || row > 7));
        //else
        return pieces[column + (row * 8)];
    }

    Piece* pieceAt(char column, char row)
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            throw std::exception("Invalid coordinates");
        else
            return pieceAt((int_fast8_t)(column - 'a'), (int_fast8_t)(row - '1'));
    }

    void setPieceAt(int_fast8_t column, int_fast8_t row, Piece* p)
    {
        assert(!(column < 0 || column>7 || row < 0 || row>7));

        pieces[column + (row * 8)] = p;
    }

    void setPieceAt(char column, char row, Piece* p)
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            throw std::exception("Invalid coordinates");
        else
            return setPieceAt((int_fast8_t)(column - 'a'), (int_fast8_t)(row - '1'), p);
    }

    void deleteAndOverwritePiece(int_fast8_t column, int_fast8_t row, Piece* p)
    {
        assert (!(column < 0 || column>7 || row < 0 || row>7));

        //delete pieces[(column - 'a') + (row - '1') * 8];
        pieces[column + (row * 8)] = p;
    }

    void deleteAndOverwritePiece(char column, char row, Piece* p)
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            throw std::exception("Invalid coordinates");

        //delete pieces[(column - 'a') + (row - '1') * 8];
        return deleteAndOverwritePiece((int_fast8_t)(column - 'a'), (int_fast8_t)(row - '1'), p);
    }

    void deleteAndMovePiece(char columnFrom, char rowFrom, char columnTo, char rowTo)
    {
        if (columnFrom < 'a' || columnFrom>'h' || rowFrom < '1' || rowFrom>'8' || columnTo < 'a' || columnTo>'h' || rowTo < '1' || rowTo>'8')
            throw std::exception("Invalid coordinates");

        const uint_fast8_t posFrom = (columnFrom - 'a') + (rowFrom - '1') * 8;
        const uint_fast8_t posTo = (columnTo - 'a') + (rowTo - '1') * 8;

        if (pieces[posFrom] == nullptr)
            throw std::exception("Trying to move empty field");

        //delete pieces[posTo];
        pieces[posTo] = pieces[posFrom];
        pieces[posFrom] = nullptr;
    }

    //void printW(PlayerSide pov = PlayerSide::WHITE) const
    //{
    //    print(std::wcout, pov);
    //}
    void print(PlayerSide pov = PlayerSide::WHITE) const
    {
        return print(out, pov);
    }

    template <class T>
    void print(T& os, PlayerSide pov = PlayerSide::WHITE) const
    {
        static_assert(std::is_base_of<std::ios_base, T>::value, "Must be an output stream");
        switch (pov)
        {
        case BLACK:
        {
            for (i8 i = 0; i < 8; ++i) {
                os << i + 1 << ' ';
                for (i8 j = 7; j >= 0; --j) {
                    os << '|';
                    if (pieces[j + i * 8] != nullptr)
                        pieces[j + i * 8]->print(os);
                    else
                    {
                        if constexpr (std::is_base_of<std::wios, T>::value)
                            os << L' ';
                        else
                            os << ' ';
                    }

                }
                os << '|' << nl;
            }
            os << "   h g f e d c b a";
        } break;
        case WHITE:
        {
            for (i8 i = 7; i >= 0; --i) {
                os << i + 1 << ' ';
                for (i8 j = 0; j < 8; ++j) {
                    os << '|';
                    //wcout<<((((j+i*8)%2)==0)?"\033[40m":"\033[43m");
                    if (pieces[j + i * 8] != nullptr)
                        pieces[j + i * 8]->print(os);
                    else
                    {
                        if constexpr (std::is_base_of<std::wios, T>::value)
                            os << L' ';
                        else
                            os << ' ';
                    }
                }
                os << '|' << nl;
            }
            os << "   a b c d e f g h";
            /*
            for (char j = 'a'; j <= 'h'; ++j) {
                wcout<<j<<L' ';
            }*/
        } break;
        default:
            std::unreachable();
        }

        os << nl << std::flush;
    }


    std::array<char, 6> findDiff(const Board& old)
    {
        std::array<char, 6> res = { 0 };

        Piece* oldPiece = nullptr;

        for (size_t i = 0; i < pieces.size(); ++i)
        {
            if (pieces[i] == nullptr && old.pieces[i] != nullptr)
            {
                oldPiece = old.pieces[i];

                res[0] = i % 8 + 'a';
                res[1] = i / 8 + '1';

                if(old.pieces[i]->pricePiece() == kingPrice)//castling hack
                    break;
            }
        }
        if (oldPiece == nullptr)
            throw std::exception("No change made");
        for (size_t i = 0; i < pieces.size(); ++i)
        {
            if (pieces[i] != nullptr && pieces[i] != old.pieces[i])
            {
                res[2] = i % 8 + 'a';
                res[3] = i / 8 + '1';

                if (tolower(oldPiece->symbolA()) == 'p' && (res[3] == '1' || res[3] == '8'))
                {
                    res[4] = tolower(pieces[i]->symbolA());
                }

                if (pieces[i]->pricePiece() == kingPrice)//castling hack
                    break;
            }
        }
        return res;
    }
    
    float balance() const {
        float res = 0;
        for (int_fast8_t i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)// && pieces[i]->pricePiece()!=kingPrice)
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
                pieces[i]->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, totalMoves, totalValues, 0,true);

                res += (((double)totalMoves)/2.0) * pieces[i]->occupancy();
                if(totalMoves>0)
                    res += std::min(std::max(totalValues,-100.0),100.0);
            }
        }
        return res / 10000.0;
    }


    //template <bool saveToVector>
    double bestMoveScore(int_fast8_t depth, double valueSoFar, double alpha, double beta)
    {
        [[assume(playerOnMove == 1 || playerOnMove == -1)]];

        if (itsTimeToStop)// [[unlikely]]
            return 0;

        //if(depth>3)
        //{
        //    auto found = transpositions.find(*this);
        //    if (found != transpositions.end())
        //        return found->second;
        //}

        
        //{
        //    std::cerr << "Found it" << nl << std::flush;
        //    print(std::cerr);
        //}

            
        double bestValue = INT16_MAX * (-1) * playerOnMove;
        double totalValues = 0;


        uint_fast16_t totalMoves = 0;
        int_fast8_t depthToPieces = depth;

        for (int_fast8_t i = 0; i < 64; ++i) {
            Piece* found = pieces[i];

            if (found == nullptr)
                continue;
            if (found->occupancy() == playerOnMove)
            {
                auto foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), depthToPieces, alpha, beta, totalMoves, totalValues, valueSoFar);

                if (foundVal * playerOnMove > bestValue * playerOnMove) {
                    bestValue = foundVal;
                }
                if (foundVal * playerOnMove == kingPrice)//Je možné vzít krále, hra skončila
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

        //if (saveToVector) [[unlikely]]
            //return -DBL_MAX;


        if (bestValue == (double)(INT16_MAX * playerOnMove * (-1))) [[unlikely]]//Nemůžu udělat žádný legitimní tah (pat nebo mat)
        {
            bool saveToVectorBackup = saveToVector;
            saveToVector = false;
            if(canTakeKing(oppositeSide(playerOnMove)))//if (round((bestMoveScore(1, onMove * (-1),valueSoFar, alpha, beta) * playerOnMove * (-1))/100) == kingPrice/100)//Soupeř je v situaci, kdy mi může vzít krále (ohrožuje ho)
            {
                bestValue = -matePrice * playerOnMove;//Dostanu mat, co nejnižší skóre
            }
            else
            {//Dostal bych pat, ten je vždycky lepší než dostat mat, ale chci ho docílit jen když prohrávám
                bestValue = 0;//When we return 0, stalemate is preffered only if losing (0 is better than negative)
                //if (valueSoFar * playerOnMove > 0)
                //{
                //    //std::cerr << "Stalemate not wanted for " << (unsigned)onMove << std::endl;
                //    bestValue = -stalePrice * playerOnMove;//Vedu, nechci dostat pat
                //}
                //else
                //{
                //    //std::cerr << "Stalemate wanted for "<< (unsigned)onMove << std::endl;
                //    bestValue = stalePrice * playerOnMove;//Prohrávám, dostat pat beru jako super tah
                //}  
            }
            bestValue *=depth;//To get shit done quickly
            saveToVector = saveToVectorBackup;
        }

        //if (depth > 3)
        //    transpositions.emplace(*this, bestValue);
        return bestValue;
    }


    double tryPiece(int_fast8_t column, int_fast8_t row, Piece* p, int_fast8_t depth, double alpha, double beta, double valueSoFar)
    {
        Piece* backup = pieceAt(column, row);
        setPieceAt(column, row, p);
        auto foundVal = bestMoveScore(depth, valueSoFar, alpha, beta);
        setPieceAt(column, row, backup);
        return foundVal;
    }

    double tryPiecePosition(int_fast8_t column, int_fast8_t row, Piece* p)
    {
        Piece* backup = pieceAt(column, row);
        setPieceAt(column, row, p);
        auto foundVal = positionScoreHeuristic();
        setPieceAt(column, row, backup);
        return foundVal;
    }

    int_fast8_t countPiecesMin() const
    {
        std::array<int_fast8_t, 2> counters{ 0 };
        for (const auto& i : pieces)
        {
            if (i != nullptr)
                ++counters[(i->occupancy() + 1)/2];
        }
        return std::min(counters[0], counters[1]);
    }
};


std::size_t BoardHasher::operator()(const Board& s) const noexcept
{
    size_t res = 0;

    for (i8 i = 0; i < s.pieces.size(); ++i)
    {
        res ^= (((size_t)s.pieces[i]) + (123*i)) * (i + 1);
    }

    return res;
}

struct GameMove {
    Board researchedBoard;
    double bestFoundValue;
    double startingValue;
    //Board movePosition;
    i8 researchedDepth;

    PlayerSide firstMoveOnMove;
    std::array<char, 6> firstMoveNotation{ '\0' };


    GameMove() = default;
    GameMove(const GameMove& copy) noexcept = default;//:researchedBoard(copy.researchedBoard),bestFoundValue(copy.bestFoundValue),pieceTakenValue(copy.pieceTakenValue){}
    GameMove(GameMove&& toMove) noexcept = default;// :researchedBoard(move(toMove.researchedBoard)), bestFoundValue(toMove.bestFoundValue), pieceTakenValue(toMove.pieceTakenValue) {}


    //GameMove& operator=(GameMove&& toMove) = default;

    GameMove& operator=(const GameMove& copy) = default;

    friend bool operator<(const GameMove& l, const GameMove& r)
    {
        return l.bestFoundValue < r.bestFoundValue;
    }
    friend bool operator>(const GameMove& l, const GameMove& r)
    {
        return l.bestFoundValue > r.bestFoundValue;
    }

    //GameMove(Board researchedBoard, double bestFoundValue, double startingValue):researchedBoard(move(researchedBoard)),bestFoundValue(bestFoundValue), startingValue(startingValue) {}
    GameMove(Board board, double startingValue) :researchedBoard(std::move(board)), bestFoundValue(startingValue), startingValue(startingValue) {}
};


static std::vector<GameMove> firstPositions;



void Piece::tryChangeAndUpdateIfBetter(Board& board, int_fast8_t column, int_fast8_t row, int_fast8_t depth, double& alpha, double& beta, double& bestValue, double& totalValues, uint_fast16_t& totalMoves, bool& doNotContinue, double valueSoFar, Piece* changeInto, double minPriceToTake, double maxPriceToTake)
{
    if (doNotContinue && !dynamicPositionRanking)
        return;

    double price = board.priceInLocation(column, row, occupancy());

    //uint_fast16_t moves = 0;

    if (price >= minPriceToTake && price <= maxPriceToTake)
    {
        if (changeInto == nullptr)
            changeInto = this;
        //int test = totalMoves;
        totalMoves += 1;
        
        totalValues += price * occupancy();

        if (saveToVector && depth==0) [[unlikely]]
        {

            //if (changeInto == nullptr)
            //    changeInto = this;
            Piece* backup = board.pieceAt(column, row);
            board.setPieceAt(column, row, changeInto);

            saveToVector = false;
            if (board.isValidSetup())
            {
                //double pieceTakenValue = valueSoFar + price * occupancy();
                float balance = board.balance();
                firstPositions.emplace_back(board, balance);
            }
            board.setPieceAt(column, row, backup);

            saveToVector = true;
            return;
        }

        if (doNotContinue)
            return;

        if (price >= kingPrice-50) [[unlikely]]//Je možné vzít krále
        {
            doNotContinue = true;
            bestValue = kingPrice * occupancy();
            //totalMoves++;
            //return;

        }
        else
        {
            if (!(price == 0 || price == -0))
            {
                //const int_fast8_t howFarFromInitial = (depthW - depth);
                const double howFarFromInitialAfterExchange = ((depthW - depth) / 2);//Kolikaty tah od initial pozice (od 0), ne pultah
                price /= (1 + howFarFromInitialAfterExchange / 100.0);
            }

            double foundVal = 0;
            double valueWithThisPiece = valueSoFar + (price * occupancy());//price * occupancy();


            //if (changeInto == nullptr)
            //    changeInto = this;


            if (depth > 0)
            {
                double foundOnly = board.tryPiece(column, row, changeInto, depth, alpha, beta, valueWithThisPiece);


                foundVal = foundOnly;

                if ((foundOnly * occupancy() * (-1)) >= kingPrice)//V dalším tahu bych přišel o krále, není to legitimní tah
                {
                    //if (foundVal > 0)
                    totalMoves -= 1;
                    return;
                }
            }
            else//konec tady, list
            {
                foundVal = valueWithThisPiece;
                if (dynamicPositionRanking)
                    foundVal += board.tryPiecePosition(column, row, changeInto);
            }


            if (foundVal * occupancy() > bestValue * occupancy())
                bestValue = foundVal;
        }

        switch (occupancy())
        {
        case(PlayerSide::WHITE): {//bily, maximalizuje hodnotu
            alpha = std::max(alpha, bestValue);
            //if (bestValue >= beta)
              //  doNotContinue = true;
        } break;
        case(PlayerSide::BLACK): {
            beta = std::min(beta, bestValue);
            //if (bestValue <= alpha)
              //  doNotContinue = true;
        } break;
        default:
            std::unreachable();
        }


        if (beta <= alpha)
        {
            doNotContinue = true;
        }
    }
}


struct Pawn : virtual public Piece {
    //virtual bool canBeEvolved(char row) const = 0;
    constexpr virtual int_fast8_t evolveRow() const = 0;

    virtual static_vector<Piece*,4>* evolveIntoReference(int_fast8_t row) const = 0;

    constexpr virtual int_fast8_t advanceRow() const = 0;

    constexpr virtual bool canGoTwoFields(int_fast8_t row) const = 0;

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
    

    constexpr float priceAdjustment(int_fast8_t column, int_fast8_t row) const final
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

    virtual double bestMoveWithThisPieceScore(Board& board, int_fast8_t column, int_fast8_t row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override
    {
        double bestValue = INT16_MAX * (-1) * occupancy();

        uint_fast16_t moves = 0;

        auto availableOptions = evolveIntoReference(row + advanceRow());


        board.setPieceAt(column, row, nullptr);
        board.playerOnMove = oppositeSide(board.playerOnMove);

        if (board.priceInLocation(column, row + advanceRow(), occupancy()) == 0)//Muzu jit pescem o jedno dopredu
        {

            for (const auto& evolveOption : *availableOptions) {
                //One field forward
                {
                    double valueDifferenceNextMove = (evolveOption->price(column, row + advanceRow()) - price(column, row)) * occupancy();
                    tryChangeAndUpdateIfBetter(board, column, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar + valueDifferenceNextMove, evolveOption, 0, 0);
                }
                if (canGoTwoFields(row))//Two fields forward
                {
                    double valueDifferenceNextMove = (evolveOption->price(column, row + advanceRow() * 2) - price(column, row)) * occupancy();
                    tryChangeAndUpdateIfBetter(board, column, row + advanceRow() * 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar + valueDifferenceNextMove, evolveOption, 0, 0);
                }

            }
        }
        for (const auto& evolveOption : *availableOptions) {
            if(column<7)
            {
                double valueDifferenceNextMove = (evolveOption->price(column + 1, row + advanceRow()) - price(column, row)) * occupancy();
                tryChangeAndUpdateIfBetter(board, column + 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar + valueDifferenceNextMove, evolveOption, DBL_MIN);
            }
            if(column>0)
            {
                double valueDifferenceNextMove = (evolveOption->price(column - 1, row + advanceRow()) - price(column, row)) * occupancy();
                tryChangeAndUpdateIfBetter(board, column - 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar + valueDifferenceNextMove, evolveOption, DBL_MIN);
            }
        }
        board.playerOnMove = oppositeSide(board.playerOnMove);
        board.setPieceAt(column, row, this);
        

        return bestValue;
    }

};


struct Knight : virtual public Piece {

    virtual double bestMoveWithThisPieceScore(Board& board, int_fast8_t column, int_fast8_t row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {

        double bestValue = INT16_MAX * (-1) * occupancy();

        //if (depth <= 0)
          //  return 0;
        board.setPieceAt(column, row, nullptr);
        board.playerOnMove = oppositeSide(board.playerOnMove);

        tryChangeAndUpdateIfBetter(board, column + 1, row + 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 1, row - 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 2, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 2, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row + 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row - 2, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 2, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 2, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);

        board.playerOnMove = oppositeSide(board.playerOnMove);
        board.setPieceAt(column, row, this);

        return bestValue;
    }


    virtual constexpr float pricePiece() const final {
        return 32;
    }

    constexpr float priceAdjustment(int_fast8_t column, int_fast8_t row) const final
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
    virtual double bestMoveWithThisPieceScore(Board& board, int_fast8_t column, int_fast8_t row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue)  override {

        double bestValue = INT16_MAX * (-1) * occupancy();
        //if (depth <= 0)
          //  return 0;

        board.setPieceAt(column, row, nullptr);
        board.playerOnMove = oppositeSide(board.playerOnMove);
        //bool foundKing = false;

        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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

        board.playerOnMove = oppositeSide(board.playerOnMove);
        board.setPieceAt(column, row, this);


        return bestValue;
    }

    virtual constexpr float pricePiece() const final {
        return 33.3;
    }

    constexpr float priceAdjustment(int_fast8_t column, int_fast8_t row) const final
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
    virtual double bestMoveWithThisPieceScore(Board& board, int_fast8_t column, int_fast8_t row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {

        double bestValue = INT16_MAX * (-1) * occupancy();

        board.setPieceAt(column, row, nullptr);
        board.playerOnMove = oppositeSide(board.playerOnMove);

        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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

        board.playerOnMove = oppositeSide(board.playerOnMove);
        board.setPieceAt(column, row, this);

        return bestValue;
    }

    virtual constexpr float pricePiece() const final {
        return 51;
    }

    constexpr float priceAdjustment(int_fast8_t column, int_fast8_t row) const final
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

    virtual double bestMoveWithThisPieceScore(Board& board, int_fast8_t column, int_fast8_t row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {
        //researchedBoard.print();

        double bestValue = INT16_MAX * (-1) * occupancy();


        Piece* originalPiece = board.pieceAt(column, row);
        board.setPieceAt(column, row, nullptr);
        board.playerOnMove = oppositeSide(board.playerOnMove);


        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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

        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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
        for (int_fast8_t i = 1; i < 8; ++i) {
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

        for (int_fast8_t i = 1; i < 8; ++i) {
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

        board.playerOnMove = oppositeSide(board.playerOnMove);
        board.setPieceAt(column, row, this);

        return bestValue;
    }


    virtual constexpr float pricePiece() const final {
        return 88;
    }

    constexpr float priceAdjustment(int_fast8_t column, int_fast8_t row) const final
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
    virtual double bestMoveWithThisPieceScore(Board& board, int_fast8_t column, int_fast8_t row, int_fast8_t depth, double& alpha, double& beta, uint_fast16_t& totalMoves, double& totalValues, double valueSoFar, bool doNoContinue) override {
        double bestValue = INT16_MAX * (-1) * occupancy();

        board.setPieceAt(column, row, nullptr);
        board.playerOnMove = oppositeSide(board.playerOnMove);

        tryChangeAndUpdateIfBetter(board, column + 1, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 1, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column + 1, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column - 1, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column, row + 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);
        tryChangeAndUpdateIfBetter(board, column, row - 1, depth - 1, alpha, beta, bestValue, totalValues, totalMoves, doNoContinue, valueSoFar);

        board.playerOnMove = oppositeSide(board.playerOnMove);
        board.setPieceAt(column, row, this);

        return bestValue;

    }

    virtual constexpr float pricePiece() const final {
        return kingPrice;
    }

    //double price(char column, char row) const override {

    //    return kingPrice;
    //}

    constexpr float priceAdjustment(int_fast8_t column, int_fast8_t row) const final
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
    constexpr PlayerSide occupancy() const final {
        return PlayerSide::WHITE;
    }
    constexpr float price(int_fast8_t column, int_fast8_t row) const final {
        return pricePiece() + priceAdjustment(column, 7-row);
    }
};
struct BlackPiece : virtual Piece {
    constexpr PlayerSide occupancy() const final {
        return PlayerSide::BLACK;
    }
    constexpr float price(int_fast8_t column, int_fast8_t row) const final {
        return pricePiece() + priceAdjustment(7-column, row);
    }
};

struct KnightWhite final :public Knight, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♘';
    }

    //Piece* clone() const override {
    //    return new KnightWhite(*this);
    //}
    constexpr char symbolA() const override {
        return 'n';
    }
} knightWhite;

struct KnightBlack final :public Knight, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♞';
    }
    //Piece* clone() const override {
    //    return new KnightBlack(*this);
    //}
    constexpr char symbolA() const override {
        return 'N';
    }
} knightBlack;

struct BishopWhite final :public Bishop, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♗';
    }
    //Piece* clone() const override {
    //    return new BishopWhite(*this);
    //}
    constexpr char symbolA() const override {
        return 'b';
    }
} bishopWhite;

struct BishopBlack final :public Bishop, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♝';
    }
    //Piece* clone() const override {
    //    return new BishopBlack(*this);
    //}
    constexpr char symbolA() const override {
        return 'B';
    }
} bishopBlack;

struct RookWhite final :public Rook, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♖';
    }
    //Piece* clone() const override {
    //    return new RookWhite(*this);
    //}
    constexpr char symbolA() const override {
        return 'r';
    }
} rookWhite;

struct RookBlack final :public Rook, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♜';
    }
    //Piece* clone() const override {
    //    return new RookBlack(*this);
    //}
    constexpr char symbolA() const override {
        return 'R';
    }
} rookBlack;
struct QueenWhite final :public Queen, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♕';
    }
    //Piece* clone() const override {
    //    return new QueenWhite(*this);
    //}
    constexpr char symbolA() const override {
        return 'q';
    }
} queenWhite;
struct QueenBlack final :public Queen, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♛';
    }
    //Piece* clone() const override {
    //    return new QueenBlack(*this);
    //}
    constexpr char symbolA() const override {
        return 'Q';
    }
} queenBlack;
struct KingWhite final :public King, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♔';
    }
    //Piece* clone() const override {
    //    return new KingWhite(*this);
    //}
    constexpr char symbolA() const override {
        return 'k';
    }
} kingWhite;
struct KingBlack final :public King, public BlackPiece {
    //char occupancy() const override {
    //    return -1;
    //}
    constexpr wchar_t symbolW() const override {
        return L'♚';
    }
    //Piece* clone() const override {
    //    return new KingBlack(*this);
    //}
    constexpr char symbolA() const override {
        return 'K';
    }
} kingBlack;


struct PawnWhite final :public Pawn, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♙';
    }
    constexpr int_fast8_t evolveRow() const override {
        return 7;
    }

    /*bool canBeEvolved(char row) const override{
        return row=='8';
    }*/

    constexpr bool canGoTwoFields(int_fast8_t row) const override {
        return row == 1;
    }

    constexpr int_fast8_t advanceRow() const override {
        return 1;
    }

    //Piece* clone() const override {
    //    return new PawnWhite(*this);
    //}
    constexpr char symbolA() const override {
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
    virtual static_vector<Piece*,4>* evolveIntoReference(int_fast8_t row) const;

} pawnWhite;
struct PawnBlack final:public Pawn, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♟';
    }
    int_fast8_t evolveRow() const override {
        return 0;
    }
    /*bool canBeEvolved(char row) const override{
        return row=='1';
    }*/
    bool canGoTwoFields(int_fast8_t row) const override {
        return row == 6;
    }
    int_fast8_t advanceRow() const override {
        return -1;
    }

    //Piece* clone() const override {
    //    return new PawnBlack(*this);
    //}
    constexpr char symbolA() const override {
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

    virtual static_vector<Piece*,4>* evolveIntoReference(int_fast8_t row) const;

} pawnBlack;

static_vector<Piece*,4> whiteEvolvePawnOnly = { &pawnWhite };
static_vector<Piece*,4> whiteEvolveLastRow = { &queenWhite, &rookWhite, &bishopWhite, &knightWhite };
static_vector<Piece*,4> blackEvolvePawnOnly = { &pawnBlack };
static_vector<Piece*,4> blackEvolveLastRow = { &queenBlack, &rookBlack, &bishopBlack, &knightBlack };

static_vector<Piece*,4>* PawnWhite::evolveIntoReference(int_fast8_t row) const {
    if (row == evolveRow()) [[unlikely]]
        return &whiteEvolveLastRow;
    else [[likely]]
        return &whiteEvolvePawnOnly;
}

static_vector<Piece*,4>* PawnBlack::evolveIntoReference(int_fast8_t row) const {
    if (row == evolveRow()) [[unlikely]]
        return &blackEvolveLastRow;
    else [[likely]]
        return &blackEvolvePawnOnly;
}

template<typename T>
inline void update_max(std::atomic<T>& atom, const T val)
{
    for (T atom_val = atom; atom_val < val && atom.compare_exchange_weak(atom_val, val););//std::memory_order_relaxed
}

template<typename T>
inline void update_min(std::atomic<T>& atom, const T val)
{
    for (T atom_val = atom; atom_val > val && atom.compare_exchange_weak(atom_val, val););//std::memory_order_relaxed
}

static std::atomic<double> alphaOrBeta;

//template <typename T>
//void printGeneralInfo(T& o)
//{
//    o << "info depth "<<depthW+1<<" time "<<time
//}
template <typename T>
T& printScore(T& o, float scoreCp, i8 depth, PlayerSide playerOnMove)
{
    o << "score ";
    int mateFound = round(abs(scoreCp) / matePrice);
    if (mateFound > 0)
    {//(1 * board.playerOnMove) + 
        int mateIn = ceil((depth - mateFound) / 2.0);
        if ((scoreCp * playerOnMove) < 0)
            mateIn *= -1;
        o << "mate " << mateIn;
        //break; //If mate is inevitable, it makes no sense to continue looking
    }
    else
        o << "cp " << round(scoreCp * 10 * playerOnMove);
    return o;
}
void printLowerBound()
{
    auto os = std::osyncstream(out);
    os << "info " << "depth " << depthW + 1 << ' ';
    printScore(os, alphaOrBeta, depthW + 1, oppositeSide(onMoveW));
    os << ' ';
    os << "lowerbound";
    os << nl << std::flush;
}
void evaluateGameMove(GameMove& board, i8 depth)//, double alpha = -DBL_MAX, double beta = DBL_MAX)
{
    Board localBoard(board.researchedBoard);

    if(options.MultiPV>1) [[unlikely]]//If we want to know multiple good moves, we cannot prune using a/B at root level
        board.bestFoundValue = localBoard.bestMoveScore(depth, board.startingValue, -DBL_MAX, DBL_MAX);
    else
    {
        switch (localBoard.playerOnMove)
        {
        case PlayerSide::BLACK: {
            board.bestFoundValue = localBoard.bestMoveScore(depth, board.startingValue, alphaOrBeta, DBL_MAX);
            update_max(alphaOrBeta, board.bestFoundValue);
            //alphaOrBeta = std::max(alphaOrBeta * 1.0, researchedBoard->bestFoundValue * 1.0);
        } break;
        case PlayerSide::WHITE: {
            board.bestFoundValue = localBoard.bestMoveScore(depth, board.startingValue, -DBL_MAX, alphaOrBeta);
            update_min(alphaOrBeta, board.bestFoundValue);
            //alphaOrBeta = std::min(alphaOrBeta * 1.0, researchedBoard->bestFoundValue * 1.0);
        } break;
        default:
            std::unreachable();
        }
        printLowerBound();
    }
}


static std::vector<GameMove*> q;
static std::atomic<size_t> qPos;

void evaluateGameMoveFromQ(size_t posInQ, i8 depth)
{
    if (itsTimeToStop) [[unlikely]]
        return;

    auto& board = *q[posInQ];
    std::osyncstream(out)
        << "info "
        << "currmove "
        << board.firstMoveNotation.data() << ' '
        << "currmovenumber "
        << (posInQ + 1)
        << nl << std::flush;
    evaluateGameMove(board, depthW);
}

void workerFromQ(size_t threadId)//, double alpha = -DBL_MAX, double beta = DBL_MAX)
{
    while (true)
    {
        size_t localPos = qPos++;
        if (localPos >= q.size()) [[unlikely]]//Stopper
            return;
        auto& tmp = *q[localPos];

        assert(tmp.researchedBoard.playerOnMove == onMoveW);

        evaluateGameMoveFromQ(localPos, depthW);
    }
}

//auto stopper = GameMove(Board(), -DBL_MAX);


void printMoveInfo(unsigned depth, const double &elapsedTotal, const GameMove& move, size_t moveRank, bool upperbound, PlayerSide pov)
{
    out
        << "info "
        << "depth " << unsigned(depth) << ' '
        << "time " << round(elapsedTotal) << ' '
        ;
    printScore(out, move.bestFoundValue, depth, pov) << ' ';
    if (upperbound)
        out << "upperbound ";
    out
        << "multipv " << moveRank << ' '
        << "pv " << move.firstMoveNotation.data()
        << nl;
}

GameMove findBestOnSameLevel(std::vector<GameMove>& boards, int_fast8_t depth)//, PlayerSide onMove)
{
    timeDepthStarted = std::chrono::high_resolution_clock::now();
    assert(!boards.empty());
    PlayerSide onMoveResearched = boards.front().researchedBoard.playerOnMove;
    depth -= boards.front().researchedDepth;

    //Check if all boards are from the same POV. Required for the a/B to work.
    for (const auto& i : boards)
        assert(i.researchedBoard.playerOnMove == onMoveResearched);

#ifdef _DEBUG
    const size_t threadCount = 1;
#else
    const size_t threadCount = std::thread::hardware_concurrency();
#endif

    if (depth > 0)
    {
        alphaOrBeta = DBL_MAX * onMoveResearched;
        //transpositions.clear();

        itsTimeToStop = false;
        onMoveW = onMoveResearched;
        depthW = depth;
        q.clear();
        qPos = 0;

        q.reserve(boards.size());
        for (auto& board : boards)
            q.push_back(&board);

        if(options.MultiPV<=1)
            evaluateGameMoveFromQ(qPos++, depthW);//It is usefull to run first pass on single core at full speed to set up alpha/Beta
        

        std::vector<std::thread> threads;
        threads.reserve(threadCount - 1);
        for (size_t i = 1; i < threadCount; ++i)
            threads.emplace_back(workerFromQ,i);

        workerFromQ(0);//Do some work on this thread

        for (auto& i : threads)
            i.join();

        q.clear();
        //transpositions.clear();
    }
    
    switch (onMoveResearched)
    {
    case PlayerSide::WHITE: {
        std::stable_sort(boards.begin(), boards.end(), std::less<GameMove>());
    } break;
    case PlayerSide::BLACK: {
        std::stable_sort(boards.begin(), boards.end(), std::greater<GameMove>());
    } break;
    default:
        std::unreachable();
    }
    
    
    auto elapsedTotal = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - timeGlobalStarted).count();


    float scoreCp = boards.front().bestFoundValue;
    std::cerr << "ScoreCP: " << scoreCp << std::endl;

    //if (itsTimeToStop)
    //    return;


    for (size_t i = boards.size()-1; i != 0; --i)
        printMoveInfo(depth + 1, elapsedTotal, boards[i],i+1, options.MultiPV == 1, oppositeSide(onMoveW));

    printMoveInfo(depth + 1, elapsedTotal, boards.front(), 1, false, oppositeSide(onMoveW));

    std::cout << std::flush;

    return boards.front();
}


void timeLimit(int milliseconds, bool * doNotStop)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    if (!*doNotStop)
        itsTimeToStop = true;
    delete doNotStop;
    
}


std::vector<GameMove> generateMoves(Board& board, PlayerSide bestForWhichSide)//, i8 depth = 1
{
    static auto rd = std::random_device{};
    static auto rng = std::default_random_engine{ rd() };
    const i8 depth = 1;
    itsTimeToStop = false;
    onMoveW = board.playerOnMove;
    depthW = 1;
    saveToVector = true;

    out << "info depth 1" << nl << std::flush;
    //transpositions.clear();
    board.bestMoveScore(1, 0, -DBL_MAX, DBL_MAX);
    //transpositions.clear();
    auto res = std::move(firstPositions);
    firstPositions.clear();

    //Add move names
    for (auto& pos : res)
    {
        //pos.startingValue *= bestForWhichSide;//To our POV
        //pos.bestFoundValue *= bestForWhichSide;//To our POV
        pos.firstMoveNotation = pos.researchedBoard.findDiff(board);
        pos.researchedDepth = depth;
        pos.firstMoveOnMove = pos.researchedBoard.playerOnMove;
        //pos.movePosition = pos.researchedBoard;
    }

    //if (depth > 1)
    //{
    //    auto firstMoves = std::move(res);
    //    res.clear();

    //    onMoveW = oppositeSide(board.playerOnMove);
    //    depthW = depth - 1;


    //    size_t i = 0;
    //    for (auto& pos : firstMoves)
    //    {
    //        pos.researchedBoard.bestMoveScore(depth - 1, pos.startingValue, -DBL_MAX, DBL_MAX);
    //        for (; i < firstPositions.size(); ++i)
    //        {
    //            auto& move = firstPositions[i];
    //            move.firstMoveNotation = pos.firstMoveNotation;
    //            move.researchedDepth = pos.researchedDepth;
    //            move.firstMoveOnMove = pos.firstMoveOnMove;
    //            //move.startingValue = pos.startingValue;
    //            //move.movePosition = pos.movePosition;
    //        }
    //    }

    //    res = std::move(firstPositions);
    //    firstPositions.clear();
    //}

    saveToVector = false;

    if (!deterministic)
    {
        std::shuffle(res.begin(), res.end(), rng);
    }

    switch (bestForWhichSide)
    {
    case PlayerSide::WHITE: {
        std::sort(res.begin(), res.end(), std::greater<GameMove>());
    } break;
    case PlayerSide::BLACK: {
        std::sort(res.begin(), res.end(), std::less<GameMove>());
    } break;
    default:
        std::unreachable();
    }

    auto elapsedTotal = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - timeGlobalStarted).count();

    for (size_t i = res.size()-1;i!=(size_t)(-1); --i)
        printMoveInfo(depth, elapsedTotal, res[i], i + 1, false, bestForWhichSide);

    std::cout << std::flush;

    return res;
}

std::pair<Board, double> findBestOnTopLevel(Board& board, i8 depth, PlayerSide onMove)
{
    auto tmp = generateMoves(board, onMove);//, onMove);
    auto res = findBestOnSameLevel(tmp, depth - 1);//, oppositeSide(onMove));
    return { res.researchedBoard, res.bestFoundValue };
}



std::string_view getWord(std::string_view& str)
{
    size_t i = 0;
    for (; i < str.size(); ++i)
    {
        if (isspace(str[i]))
            break;
    }
    auto res = str.substr(0, i);
    if (i >= str.size())
        str = "";
    else
        str = str.substr(i + 1);
    return res;
}

void executeMove(Board& board, std::string_view& str)
{
    auto move = getWord(str);

    auto backup = board.pieceAt(move[2], move[3]);
    board.deleteAndMovePiece(move[0], move[1], move[2], move[3]);

    if (move == "e1c1" && board.pieceAt(move[2], move[3]) == &kingWhite)
        board.deleteAndMovePiece('a', '1', 'd', '1');
    else if (move == "e1g1" && board.pieceAt(move[2], move[3]) == &kingWhite)
        board.deleteAndMovePiece('h', '1', 'f', '1');
    else if (move == "e8c8" && board.pieceAt(move[2], move[3]) == &kingBlack)
        board.deleteAndMovePiece('a', '8', 'd', '8');
    else if (move == "e8g8" && board.pieceAt(move[2], move[3]) == &kingBlack)
        board.deleteAndMovePiece('h', '8', 'f', '8');

    else if (move.size() == 5)
    {
        Piece* evolvedInto = nullptr;
        if (move[3] == '8' && move[4] == 'q')
            evolvedInto = &queenWhite;
        else if (move[3] == '1' && move[4] == 'q')
            evolvedInto = &queenBlack;
        else if (move[3] == '8' && move[4] == 'r')
            evolvedInto = &rookWhite;
        else if (move[3] == '1' && move[4] == 'r')
            evolvedInto = &rookBlack;
        else if (move[3] == '8' && move[4] == 'b')
            evolvedInto = &bishopWhite;
        else if (move[3] == '1' && move[4] == 'b')
            evolvedInto = &bishopBlack;
        else if (move[3] == '8' && move[4] == 'k')
            evolvedInto = &knightWhite;
        else if (move[3] == '1' && move[4] == 'k')
            evolvedInto = &knightBlack;
        board.deleteAndOverwritePiece(move[2], move[3], evolvedInto);
    }
    else if (move[1] == '5' && board.pieceAt(move[2], move[3]) == &pawnWhite && move[0] != move[2] && backup == nullptr)//Bily tah mimochodem
    {
        board.deleteAndOverwritePiece(move[2], move[3] - 1, nullptr);
    }
    else if (move[1] == '4' && board.pieceAt(move[2], move[3]) == &pawnBlack && move[0] != move[2] && backup == nullptr)//Bily tah mimochodem
    {
        board.deleteAndOverwritePiece(move[2], move[3] + 1, nullptr);
    }

    board.playerOnMove = oppositeSide(board.playerOnMove);
}

void parseMoves(Board& board, std::string_view str)
{
    if (getWord(str) != "moves")
        return;

    while (!str.empty())
    {
        executeMove(board, str);
    }
}


Board startingPosition()
{
    Board initial;
    initial.playerOnMove = PlayerSide::WHITE;

    initial.deleteAndOverwritePiece('a', '1', &rookWhite);
    initial.deleteAndOverwritePiece('b', '1', &knightWhite);
    initial.deleteAndOverwritePiece('c', '1', &bishopWhite);
    initial.deleteAndOverwritePiece('d', '1', &queenWhite);
    initial.deleteAndOverwritePiece('e', '1', &kingWhite);
    initial.deleteAndOverwritePiece('f', '1', &bishopWhite);
    initial.deleteAndOverwritePiece('g', '1', &knightWhite);
    initial.deleteAndOverwritePiece('h', '1', &rookWhite);

    initial.deleteAndOverwritePiece('a', '2', &pawnWhite);
    initial.deleteAndOverwritePiece('b', '2', &pawnWhite);
    initial.deleteAndOverwritePiece('c', '2', &pawnWhite);
    initial.deleteAndOverwritePiece('d', '2', &pawnWhite);
    initial.deleteAndOverwritePiece('e', '2', &pawnWhite);
    initial.deleteAndOverwritePiece('f', '2', &pawnWhite);
    initial.deleteAndOverwritePiece('g', '2', &pawnWhite);
    initial.deleteAndOverwritePiece('h', '2', &pawnWhite);

    initial.deleteAndOverwritePiece('a', '8', &rookBlack);
    initial.deleteAndOverwritePiece('b', '8', &knightBlack);
    initial.deleteAndOverwritePiece('c', '8', &bishopBlack);
    initial.deleteAndOverwritePiece('d', '8', &queenBlack);
    initial.deleteAndOverwritePiece('e', '8', &kingBlack);
    initial.deleteAndOverwritePiece('f', '8', &bishopBlack);
    initial.deleteAndOverwritePiece('g', '8', &knightBlack);
    initial.deleteAndOverwritePiece('h', '8', &rookBlack);

    initial.deleteAndOverwritePiece('a', '7', &pawnBlack);
    initial.deleteAndOverwritePiece('b', '7', &pawnBlack);
    initial.deleteAndOverwritePiece('c', '7', &pawnBlack);
    initial.deleteAndOverwritePiece('d', '7', &pawnBlack);
    initial.deleteAndOverwritePiece('e', '7', &pawnBlack);
    initial.deleteAndOverwritePiece('f', '7', &pawnBlack);
    initial.deleteAndOverwritePiece('g', '7', &pawnBlack);
    initial.deleteAndOverwritePiece('h', '7', &pawnBlack);
    return initial;
}

Board posFromString(std::string_view str)
{
    if (getWord(str) == "startpos")
    {
        Board res = startingPosition();
        parseMoves(res, str);

#ifdef _DEBUG
        res.print(std::cerr);
#endif
        return res;
    }
    throw;//not implemented

}


GameMove findBestInTimeLimit(Board& board, int milliseconds, bool endSooner = true)
{
    //dynamicPositionRanking = false;
    timeGlobalStarted = std::chrono::high_resolution_clock::now();

    bool* tellThemToStop = new bool;
    *tellThemToStop = false;

    auto limit = std::thread(timeLimit, milliseconds,tellThemToStop);
    //auto start = chrono::high_resolution_clock::now();

    auto boardList = generateMoves(board,board.playerOnMove);
    //for (size_t i = 0; i < boardList.size(); i++)
    //{
    //    wcout << boardList[i].pieceTakenValue << endl;
    //    boardList[i].researchedBoard.print();
    //}

    GameMove res;
    //dynamicPositionRanking = true;

    std::wcout << "Depth: ";
    for (int_fast8_t i = 4; i < 100; i += 2) {
        auto bestPosFound = findBestOnSameLevel(boardList, i);//, oppositeSide(researchedBoard.playerOnMove));
        //dynamicPositionRanking = false;
        if (itsTimeToStop)
        {
            limit.join();
            break;
        }

        res = bestPosFound;
        std::wcout << i << ' ';

        if (endSooner)
        {
            double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeGlobalStarted).count();
            double remaining = milliseconds - elapsed;

            if (remaining/4.0  < elapsed)
            {
                std::wcout << std::endl << "Would not likely finish next stage, optimizing out " << remaining / 1000 << "s.";
                itsTimeToStop = true;
                *tellThemToStop = true;
                limit.detach();//prasarna
                break;
            }
        }

        
    }
    std::wcout << std::endl;

    return res;
}

GameMove findBestInNumberOfMoves(Board& board, int_fast8_t moves)
{
    timeGlobalStarted = std::chrono::high_resolution_clock::now();
    //dynamicPositionRanking = false;
    auto boardList = generateMoves(board,board.playerOnMove);

    //for (const auto& i : boardList)
    //{
    //    out << i.bestFoundValue << std::endl;
    //    i.researchedBoard.print();
    //    out << std::endl;
    //}
    //boardList.erase(boardList.begin());

    GameMove res;
    //dynamicPositionRanking = true;

    std::wcout << "Depth: ";
    for (int_fast8_t i = 4; i <= moves; i += 2) {
        auto bestPosFound = findBestOnSameLevel(boardList, i);
        //dynamicPositionRanking = false;
        if (itsTimeToStop)
            break;
        else
        {
            res = bestPosFound;
        }
        std::wcout << i << ' ';
    }
    std::wcout << std::endl;

    return res;
}


void benchmark(char depth = 8, Board board = startingPosition())
{
    board.print();
    auto start = std::chrono::high_resolution_clock::now();
    //doneMoves.clear();
    //auto result = findBestOnTopLevel(researchedBoard,depth,onMove);
    //auto result = findBestInTimeLimit(researchedBoard, onMove, timeToPlay);
    auto result = findBestInNumberOfMoves(board, depth);
    auto end = std::chrono::high_resolution_clock::now();

    //std::string_view move = result.firstMoveNotation.data();
    //executeMove(board, move);
    std::wcout << result.firstMoveNotation.data() << std::endl;

    std::wcout << "cp: " << result.bestFoundValue << std::endl;//<<"Total found score "<<result.second+result.first.balance()<<endl;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
    std::wcout << "Done in " << elapsed << "s." << std::endl;

    //board.printW();
}


long long boardUserInput(Board& board, PlayerSide printToSide)
{

    std::string tmp;
    auto startHuman = std::chrono::high_resolution_clock::now();
    std::cin >> tmp;
    auto endHuman = std::chrono::high_resolution_clock::now();


    while (true)
    {
        try
        {
            if ((tmp.length()-4)%5!=0)
                throw std::exception("bad input");

            for (size_t i = 0; i < tmp.length(); i+=5)
            {
                if (tmp[0+i] == 'w' && tmp[1+i] == 'q')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], &queenWhite);
                else if (tmp[0 + i] == 'b' && tmp[1 + i] == 'q')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], &queenBlack);
                else if (tmp[0 + i] == 'w' && tmp[1 + i] == 'r')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], &rookWhite);
                else if (tmp[0 + i] == 'b' && tmp[1 + i] == 'r')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], &rookBlack);
                else if (tmp[0 + i] == 'w' && tmp[1 + i] == 'b')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], &bishopWhite);
                else if (tmp[0 + i] == 'b' && tmp[1 + i] == 'b')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], &bishopBlack);
                else if (tmp[0 + i] == 'w' && tmp[1 + i] == 'k')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], &knightWhite);
                else if (tmp[0 + i] == 'b' && tmp[1 + i] == 'k')
                    board.deleteAndOverwritePiece(tmp[2 + i], tmp[3 + i], &knightBlack);
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
            out << e.what() << std::endl;
            std::cin >> tmp;
        }
    }


    auto elapsedHuman = std::chrono::duration_cast<std::chrono::milliseconds>(endHuman - startHuman).count();
    std::wcout << "Human moved in " << elapsedHuman / 1000.0 << "s." << std::endl;

    board.print(printToSide);
    return elapsedHuman + 10;
}


void playGameInTime(Board board, PlayerSide onMove, int timeToPlay)
{
    board.print(onMove);
    if (onMove == PlayerSide::BLACK)
    {
        boardUserInput(board,onMove);
    }
    board.playerOnMove = onMove;
    
    while (true)
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto balanceBefore = board.balance();
        auto result = findBestInTimeLimit(board, timeToPlay);
        std::string_view move = result.firstMoveNotation.data();
        executeMove(board, move);
        std::wcout << "Score change: " << result.bestFoundValue << std::endl<<"Score result: "<< result.bestFoundValue + balanceBefore << std::endl;
        auto end = std::chrono::high_resolution_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
        std::wcout << "Done in " << elapsed << "s." << std::endl;
        board.print(onMove);

        while (isspace(std::cin.peek()))
            std::cin.get();

        char op = std::cin.get();

        if (op == '+' || op == '-' || op == '=')
        {
            int seconds;
            std::cin >> seconds;
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
            std::wcout << "Time for move changed to " << timeToPlay << "ms." << std::endl;
        }
        else
        {
            std::cin.putback(op);
        }

        
        boardUserInput(board, onMove);

    }
}



void playGameResponding(Board board, PlayerSide onMove)
{
    long long milliseconds = 5000;
    if (onMove == PlayerSide::BLACK)
    {
        milliseconds = boardUserInput(board, onMove);
    }

    while (true)
    {
        auto start = std::chrono::high_resolution_clock::now();
        //doneMoves.clear();
        board.playerOnMove = onMove;
        auto result = findBestInTimeLimit(board, milliseconds);
        //auto result = findBestInNumberOfMoves(researchedBoard, onMove, 8);
        auto end = std::chrono::high_resolution_clock::now();
        std::string_view move = result.firstMoveNotation.data();
        executeMove(board, move);
        std::wcout << "Best position found score change: " << result.bestFoundValue << std::endl;//<<"Total found score "<<result.second+result.first.balance()<<endl;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
        std::wcout << "PC answered in " << elapsed << "s." << std::endl;

        board.print();
        milliseconds = boardUserInput(board, onMove);
    }
}


int uci()
{
    {
        std::string command;
        std::getline(std::cin, command);
        if (command != "uci")
        {
            std::cerr << "Only UCI protocol is supported in this configuration." << std::endl;
            return -1;
        }
    }

    out << "id name Klara Destroyer" << nl
        << "id author Matej Kocourek" << nl
        << "option name MultiPV type spin min 1 default " << options.MultiPV << nl
        << "uciok" << nl
        << std::flush;


    //while (true)
    //{
        //std::string command;
        //std::cin >> command;
        //if (command == "isready")
        //{
        //    out << "readyok" << std::endl;
    Board board;
    //std::ofstream debugOut("debug.log");

    while (true)
    {
        std::string command;
        std::getline(std::cin, command);
        //debugOut << command << std::endl;

        std::string_view commandView(command);
        auto commandFirst = getWord(commandView);
        if (commandFirst == "isready")
        {
            board = Board();
            out << "readyok" << nl << std::flush;
        }
        else if (commandFirst == "quit")
        {
            std::cerr << "Bye!" << std::endl;
            return 0;
        }
        else if (commandFirst == "position")
        {
            board = posFromString(commandView);
        }
        else if (commandFirst == "setoption")
        {
            if (getWord(commandView) != "name")
                continue;
            auto optionName = getWord(commandView);
            auto optionValue = getWord(commandView);
            if (optionName == "MultiPV")
            {
                options.MultiPV = std::atoll(optionValue.data());
                std::cerr << "Setting MultiPV to " << options.MultiPV << std::endl;
            }
        }
        else if (commandFirst == "go")
        {
            timeGlobalStarted = std::chrono::high_resolution_clock::now();

            int64_t wtime = 0, btime = 0, winc = 0, binc = 0;

            while (true)
            {
                auto word = getWord(commandView);
                if (word.empty())
                    break;
                if (word == "wtime")
                    wtime = atoi(getWord(commandView).data());
                if (word == "btime")
                    btime = atoi(getWord(commandView).data());
                if (word == "winc")
                    winc = atoi(getWord(commandView).data());
                if (word == "binc")
                    binc = atoi(getWord(commandView).data());
            }

            float gamePhase = (17.0f - board.countPiecesMin()) / 16.0f;

            int64_t myTime = board.playerOnMove == 1 ? wtime : btime;
            int64_t myInc = board.playerOnMove == 1 ? winc : binc;

            int64_t timeTargetOptimal = myInc + myTime * gamePhase / 6;
            int64_t timeTargetMax = myInc + myTime * gamePhase / 3;

            std::cerr << "Targeting " << timeTargetOptimal << " ms." << std::endl;
            std::cerr << "Highest I can go is " << timeTargetMax << " ms." << std::endl;


            //dynamicPositionRanking = false;
            auto boardList = generateMoves(board,board.playerOnMove);
            //dynamicPositionRanking = true;

            GameMove bestPosFound;

            std::vector<std::pair<double, float>> previousResults;

            //std::cerr << "Depth: ";

            for (int_fast8_t i = 2; /*i < 100*/; i += 2) {
                out << "info " << "depth " << unsigned(i) << nl << std::flush;

                bestPosFound = findBestOnSameLevel(boardList, i);
                auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - timeDepthStarted).count();
                //chrono::duration_cast<chrono::milliseconds>();
                auto elapsedTotal = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - timeGlobalStarted).count();

                float scoreCp = bestPosFound.bestFoundValue;


                if (round(abs(scoreCp) / matePrice) > 0)
                    break;

                //dynamicPositionRanking = false;

                //res = bestPosFound;
                //std::cerr << "Elapsed time for ";
                //std::cerr << i + 1 << ' ';


                //std::cerr << " is " << elapsed << std::endl;

                if (elapsedTotal >= timeTargetMax)//Emergency stop if we depleted time
                    break;
                previousResults.emplace_back(elapsed, bestPosFound.bestFoundValue);

                if (previousResults.size() >= 2)
                {
                    double logOlder = log2(previousResults[previousResults.size() - 2].first);
                    double logNewer = log2(previousResults[previousResults.size() - 1].first);
                    double growth = logNewer + logNewer - logOlder;

                    constexpr double branching = 1.5;
                    growth *= branching;

                    //double growth = pow(log2(previousResults[previousResults.size() - 1].first),2) / log2(previousResults[previousResults.size() - 2].first);
                    //std::cerr << "log is " << growth << std::endl;
                    double projectedNextTime = pow(2, growth);
                    std::cerr << "Projected next time is " << projectedNextTime << " ms." << std::endl;

                    if (projectedNextTime > (timeTargetMax - elapsedTotal))//.
                    {
                        std::cerr << "We wouldn't get a result in required time" << std::endl;
                        break;
                    }

                    if (projectedNextTime <= (timeTargetOptimal - elapsedTotal))//
                    {
                        std::cerr << "We are easily gonna fit in the optimal time frame" << std::endl;
                        continue;
                    }

                    //Here we calculate if it makes sense to do more calculations.
                    //It makes sense to do more if the results are unstable

                    float diff = std::abs(previousResults[previousResults.size() - 1].second - previousResults[previousResults.size() - 2].second);

                    std::cerr << "Last two diff is " << diff << std::endl;

                    if (diff < 10)//The difference is too small, we probably wouldn't get much further info.
                    {
                        std::cerr << "We could start another depth, but it's probably not necessary." << std::endl;
                        break;
                    }
                    std::cerr << "This time, we are using some extra time to really think about this move." << std::endl;
                }
            }
            //std::cerr << std::endl;
            out << "bestmove " << bestPosFound.firstMoveNotation.data() << nl << std::flush;

        }
        else
        {
            std::cerr << "Not recognized command, ignoring: " << commandFirst << std::endl;
        }
    }
    //}
//}
}

int main(int argc, char** argv) {
    std::ios_base::sync_with_stdio(false);
    //std::cerr.sync_with_stdio(false);
    if (argc > 1)
    {
        //out.sync_with_stdio(false);
        //out.precision(2);
#if out == std::wcout
        init_locale();
#endif

        std::string_view argument(argv[1]);
        if (argument == "bench")
        {
            deterministic = true;
            if (argc < 3)
                return 1;
            int n = std::atoi(argv[2]);
            benchmark(n);
        }
        else if (argument == "timed")
        {
            deterministic = false;
            std::wcout << "White/Black? [w/b]" << std::endl;
            std::string color;
            std::cin >> color;

            PlayerSide side = color[0] == 'w' ? PlayerSide::WHITE : PlayerSide::BLACK;

            playGameInTime(startingPosition(), side, 10000);
        }
        else if (argument == "imitating")
        {
            deterministic = false;
            std::wcout << "White/Black? [w/b]" << std::endl;
            std::string color;
            std::cin >> color;

            PlayerSide side = color[0] == 'w' ? PlayerSide::WHITE : PlayerSide::BLACK;

            playGameResponding(startingPosition(), side);
        }
#ifdef _DEBUG
        else if (argument == "test")
        {
            deterministic = true;
            if (argc < 3)
                return 1;
            int n = std::atoi(argv[2]);
            
            switch (n)
            {
            case(1):
            {
                Board promotion;
                promotion.deleteAndOverwritePiece('h', '5', &pawnBlack);
                promotion.deleteAndOverwritePiece('d', '5', &pawnBlack);
                promotion.deleteAndOverwritePiece('f', '5', &kingBlack);
                promotion.deleteAndOverwritePiece('g', '1', &bishopBlack);
                promotion.deleteAndOverwritePiece('e', '2', &kingWhite);
                promotion.deleteAndOverwritePiece('b', '5', &pawnWhite);
                promotion.deleteAndOverwritePiece('a', '6', &pawnWhite);

                promotion.print();

                playGameResponding(promotion, PlayerSide::WHITE);
            } break;
            case (2):
            {
                Board testMatu;
                testMatu.deleteAndOverwritePiece('h', '8', &kingBlack);
                testMatu.deleteAndOverwritePiece('a', '1', &kingWhite);
                testMatu.deleteAndOverwritePiece('g', '1', &rookWhite);
                testMatu.deleteAndOverwritePiece('a', '7', &rookWhite);
                testMatu.deleteAndOverwritePiece('b', '1', &queenWhite);
                testMatu.deleteAndOverwritePiece('c', '7', &pawnWhite);
                testMatu.playerOnMove = PlayerSide::WHITE;
                testMatu.print();

                benchmark(8, testMatu);
            } break;
            case (3):
            {
                Board testMatu;
                testMatu.deleteAndOverwritePiece('h', '8', &kingBlack);
                testMatu.deleteAndOverwritePiece('h', '7', &pawnWhite);
                testMatu.deleteAndOverwritePiece('g', '6', &pawnWhite);
                testMatu.deleteAndOverwritePiece('h', '6', &kingWhite);
                testMatu.deleteAndOverwritePiece('h', '5', &pawnWhite);
                testMatu.deleteAndOverwritePiece('g', '5', &pawnWhite);

                //testMatu.deleteAndOverwritePiece('h', '4', &kingWhite);
                //testMatu.deleteAndOverwritePiece('h', '3', &pawnWhite);
                //testMatu.deleteAndOverwritePiece('g', '3', &pawnWhite);
                //testMatu.deleteAndOverwritePiece('g', '4', &pawnWhite);

                testMatu.playerOnMove = PlayerSide::WHITE;
                testMatu.print();

                benchmark(8, testMatu);
            } break;

            default:
                break;
            }
        }
#endif
        else
        {
            std::cerr << "Unknown argument" << std::endl;
            return 1;
        }
    }
    else
    {
        deterministic = false;
        return uci();
    }
        

    return 0;
}