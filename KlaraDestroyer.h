
#include <array>
#include <string>
#include <syncstream>
#include <fstream>
#include <random>
#include <cassert>
#include <unordered_map>
#include <iostream>
//#include <vector>
#include <queue>
#include <thread>
#include <cfloat>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <barrier>
#include <future>
#include "stack_vector.h"
#include "stack_string.h"

//std::cout



namespace KlaraDestroyer {
std::ostream* outStream;
#define out (*outStream)
#define nl '\n'

#define debugOut std::cerr
//#ifdef _DEBUG std::cerr #else (void) #endif

#ifdef _DEBUG
#define AssertAssume(...)  assert(__VA_ARGS__)
#else
#define AssertAssume(...)  __assume(__VA_ARGS__)
#endif



//#define CASTLING_DISABLED
#define RELAX_CASTLING_PREDICTIONS


typedef int_fast8_t i8;
typedef uint_fast8_t u8;
typedef int_fast16_t i16;
typedef uint_fast16_t u16;
typedef int_fast32_t i32;
typedef uint_fast32_t u32;
typedef int_fast64_t i64;
typedef uint_fast64_t u64;

typedef std::chrono::duration<double, std::milli> duration_t;
typedef stack_string<5> moveNotation;

class GameState;

//static constexpr float kingPrice = 20000;
static constexpr float matePrice = 10000000;
//static constexpr float stalePrice = 50000;

static constexpr size_t maxMoves = 218;

#define MOVE_PIECE_FREE_ONLY std::equal_to<float>()
#define MOVE_PIECE_CAPTURE_ONLY std::greater<float>()
#define MOVE_PIECE_FREE_CAPTURE std::greater_equal<float>()

enum class PlayerSide : i8
{
    WHITE = 1,
    BLACK = -1,
    NONE = 0
};

enum class PieceGeneric : i8
{
    Nothing = 0,
    Pawn = 1,//0x01,
    Knight = 2,//0x02,
    Bishop = 3,//0x03,
    Rook = 4,//0x04,
    Queen = 5,//0x05,
    King = 6,//0x06,
};

enum class Piece : i8
{
    Nothing = 0,
    PawnWhite = 1,//0x01,
    KnightWhite = 2,//0x02,
    BishopWhite = 3,//0x03,
    RookWhite = 4,//0x04,
    QueenWhite = 5,//0x05,
    KingWhite = 6,//0x06,
    PawnBlack = -1,//0x81,
    KnightBlack = -2,//0x82,
    BishopBlack = -3,//0x83,
    RookBlack = -4,//0x84,
    QueenBlack = -5,//0x85,
    KingBlack = -6,//0x86
};

constexpr float operator * (float lhs, PlayerSide rhs) noexcept
{
    return lhs * static_cast<i8>(rhs);
}
constexpr double operator * (double lhs, PlayerSide rhs) noexcept
{
    return lhs * static_cast<i8>(rhs);
}
constexpr int8_t operator * (int8_t lhs, PlayerSide rhs) noexcept
{
    return lhs * static_cast<i8>(rhs);
}
constexpr int16_t operator * (int16_t lhs, PlayerSide rhs) noexcept
{
    return lhs * static_cast<i8>(rhs);
}
constexpr int32_t operator * (int32_t lhs, PlayerSide rhs) noexcept
{
    return lhs * static_cast<i8>(rhs);
}
constexpr int64_t operator * (int64_t lhs, PlayerSide rhs) noexcept
{
    return lhs * static_cast<i8>(rhs);
}

constexpr inline PlayerSide operator - (PlayerSide side) noexcept
{
    AssertAssume(static_cast<i8>(side) == 1 || static_cast<i8>(side) == -1);
    return static_cast<PlayerSide>(-static_cast<i8>(side));
}
constexpr inline PlayerSide oppositeSide(PlayerSide side) noexcept
{
    return -side;
}

constexpr inline i8 index(PlayerSide side) noexcept
{
    AssertAssume(static_cast<i8>(side) == 1 || static_cast<i8>(side) == -1);
    return (static_cast<i8>(side) + static_cast<i8>(1)) / static_cast<i8>(2);
}

constexpr inline i8 playerDirection(PlayerSide side) noexcept
{
    AssertAssume(static_cast<i8>(side) == 1 || static_cast<i8>(side) == -1);
    return static_cast<i8>(side);
}


constexpr PlayerSide pieceColor(Piece p)
{
    return static_cast<PlayerSide>(((static_cast<int8_t>(p) & static_cast <int8_t>(0x80)) >> 6) | 0x01);
}
constexpr PieceGeneric toGenericPiece(Piece p)
{
    return (PieceGeneric)((static_cast<i8>(p) >= 0) ? static_cast<i8>(p) : -static_cast<i8>(p));
    //return static_cast<PieceGeneric>(std::abs(p));
}
constexpr Piece operator * (Piece lhs, PlayerSide rhs) noexcept
{
    return static_cast<Piece>(static_cast<i8>(lhs) * static_cast<i8>(rhs));
}
constexpr Piece operator * (PieceGeneric lhs, PlayerSide rhs) noexcept
{
    return static_cast<Piece>(static_cast<i8>(lhs) * static_cast<i8>(rhs));
}

constexpr Piece fromGenericPiece(PieceGeneric p, PlayerSide side)
{
    return static_cast<Piece>(p * side);
}
constexpr inline Piece operator - (Piece piece) noexcept
{
    return static_cast<Piece>(-static_cast<i8>(piece));
}


static std::atomic<bool> criticalTimeDepleted;
static std::atomic<bool> optimalTimeDepleted;
static constexpr bool dynamicPositionRanking = false;//Not needed, static ranking is faster and should be enough
static i8 fullDepth;
static i8 availableMoves;
static PlayerSide onMoveW;
static bool shuffle;
static std::chrono::steady_clock::time_point timeGlobalStarted;
//static std::chrono::steady_clock::time_point timeDepthStarted;
static std::atomic<float> alphaOrBeta;
static constexpr i8 depthToStopOrderingPieces = 3;
static constexpr i8 depthToStopOrderingMoves = 3;

// Relax castling restrictions after certain depth. 0=full rules check only for next engines move, 1=also full check enemy move, 2=also full check engines second move, etc.
// Set to at least 1 (to avoid obvious checkmate possibility)
static constexpr i8 castlingMaxDepth = 2; 

template<typename T>
inline void update_max(std::atomic<T>& atom, const T& val)
{
    for (T atom_val = atom; atom_val < val && !atom.compare_exchange_weak(atom_val, val););//std::memory_order_relaxed
}

template<typename T>
inline void update_min(std::atomic<T>& atom, const T& val)
{
    for (T atom_val = atom; atom_val > val && !atom.compare_exchange_weak(atom_val, val););//std::memory_order_relaxed
}


struct Options {
    size_t MultiPV;
    size_t Threads;
    size_t Verbosity;
    bool UCI_Chess960;
} options;

bool firstLevelPruning = true;


constexpr i8 oneColumn = 1;
constexpr inline i8 column(i8 indexOnBoard) noexcept
{
    return indexOnBoard % 8;
}
constexpr i8 oneRow = 8;
constexpr inline i8 row(i8 indexOnBoard) noexcept
{
    return indexOnBoard / 8;
}
constexpr inline i8 toIndex(i8 column, i8 row) noexcept
{
    return row * oneRow + column * oneColumn;
}


template <typename T>
class TempSwap
{
    T& toSwap;
    T backup;
public:
    TempSwap(T& toSwap, T tempNewValue) : toSwap(toSwap), backup(toSwap)
    {
        toSwap = std::forward<T>(tempNewValue);
    }
    ~TempSwap()
    {
        toSwap = backup;
    }
};



constexpr wchar_t symbolW(Piece p)
{
    switch (p)
    {
    case Piece::Nothing:
        return L' ';
    case Piece::PawnWhite:
        return L'♙';
    case Piece::KnightWhite:
        return L'♘';
    case Piece::BishopWhite:
        return  L'♗';
    case Piece::RookWhite:
        return L'♖';
    case Piece::QueenWhite:
        return L'♕';
    case Piece::KingWhite:
        return L'♔';
    case Piece::PawnBlack:
        return L'♟';
    case Piece::KnightBlack:
        return L'♞';
    case Piece::BishopBlack:
        return L'♝';
    case Piece::RookBlack:
        return L'♜';
    case Piece::QueenBlack:
        return L'♛';
    case Piece::KingBlack:
        return L'♚';
    default:
        std::unreachable();
    }
}

constexpr char symbolA(Piece p)
{
    switch (p)
    {
    case Piece::Nothing:
        return ' ';
    case Piece::PawnWhite:
        return 'P';
    case Piece::KnightWhite:
        return 'N';
    case Piece::BishopWhite:
        return 'B';
    case Piece::RookWhite:
        return 'R';
    case Piece::QueenWhite:
        return 'Q';
    case Piece::KingWhite:
        return 'K';
    case Piece::PawnBlack:
        return 'p';
    case Piece::KnightBlack:
        return 'n';
    case Piece::BishopBlack:
        return 'b';
    case Piece::RookBlack:
        return 'r';
    case Piece::QueenBlack:
        return 'q';
    case Piece::KingBlack:
        return 'k';
    default:
        std::unreachable();
    }
}
constexpr char symbolA(PieceGeneric p)
{
    switch (p)
    {
    case PieceGeneric::Nothing:
        return ' ';
    case PieceGeneric::Pawn:
        return 'p';
    case PieceGeneric::Knight:
        return 'n';
    case PieceGeneric::Bishop:
        return 'b';
    case PieceGeneric::Rook:
        return 'r';
    case PieceGeneric::Queen:
        return 'q';
    case PieceGeneric::King:
        return 'k';
    default:
        std::unreachable();
    }
}

constexpr Piece fromSymbol(char a)
{
    switch (a)
    {
    case ' ':
        return Piece::Nothing;
    case 'P':
        return Piece::PawnWhite;
    case 'N':
        return Piece::KnightWhite;
    case 'B':
        return Piece::BishopWhite;
    case 'R':
        return Piece::RookWhite;
    case 'Q':
        return Piece::QueenWhite;
    case 'K':
        return Piece::KingWhite;
    case 'p':
        return Piece::PawnBlack;
    case 'n':
        return Piece::KnightBlack;
    case 'b':
        return Piece::BishopBlack;
    case 'r':
        return Piece::RookBlack;
    case 'q':
        return Piece::QueenBlack;
    case 'k':
        return Piece::KingBlack;
    default:
        std::unreachable();
    }
}

constexpr PieceGeneric fromGenericSymbol(char a)
{
    switch (a)
    {
    case ' ':
        return PieceGeneric::Nothing;
    case 'p':
        return PieceGeneric::Pawn;
    case 'n':
        return PieceGeneric::Knight;
    case 'b':
        return PieceGeneric::Bishop;
    case 'r':
        return PieceGeneric::Rook;
    case 'q':
        return PieceGeneric::Queen;
    case 'k':
        return PieceGeneric::King;
    default:
        std::unreachable();
    }
}



constexpr float priceAdjustment(PieceGeneric p, i8 pieceIndex)
{
    //AssertAssume(column < 8 && row < 8 && column >= 0 && row >= 0);
    switch (p)
    {
    case PieceGeneric::Nothing:
        return 0;
    case PieceGeneric::Pawn:
    {
        constexpr std::array<float, 64> arr = {
0,   0,   0,   0,   0,   0,  0,   0,
98, 134,  61,  95,  68, 126, 34, -11,
-6,   7,  26,  31,  65,  56, 25, -20,
-14,  13,   6,  21,  23,  12, 17, -23,
-27,  -2,  -5,  12,  17,   6, 10, -25,
-26,  -4,  -4, -10,   3,   3, 33, -12,
-35,  -1, -20, -23, -15,  24, 38, -22,
  0,   0,   0,   0,   0,   0,  0,   0,
        };
        return arr[pieceIndex];
    }
    case PieceGeneric::Knight:
    {
        constexpr std::array<float, 64> arr = {
-167, -89, -34, -49,  61, -97, -15, -107,
 -73, -41,  72,  36,  23,  62,   7,  -17,
 -47,  60,  37,  65,  84, 129,  73,   44,
  -9,  17,  19,  53,  37,  69,  18,   22,
 -13,   4,  16,  13,  28,  19,  21,   -8,
 -23,  -9,  12,  10,  19,  17,  25,  -16,
 -29, -53, -12,  -3,  -1,  18, -14,  -19,
-105, -21, -58, -33, -17, -28, -19,  -23,
        };
        return arr[pieceIndex];
    }
    case PieceGeneric::Bishop:
    {
        constexpr std::array<float, 64> arr = {
-29,   4, -82, -37, -25, -42,   7,  -8,
-26,  16, -18, -13,  30,  59,  18, -47,
-16,  37,  43,  40,  35,  50,  37,  -2,
 -4,   5,  19,  50,  37,  37,   7,  -2,
 -6,  13,  13,  26,  34,  12,  10,   4,
  0,  15,  15,  15,  14,  27,  18,  10,
  4,  15,  16,   0,   7,  21,  33,   1,
-33,  -3, -14, -21, -13, -12, -39, -21,
        };
        return arr[pieceIndex];
    }
    case PieceGeneric::Rook:
    {
        constexpr std::array<float, 64> arr = {
32,  42,  32,  51, 63,  9,  31,  43,
27,  32,  58,  62, 80, 67,  26,  44,
-5,  19,  26,  36, 17, 45,  61,  16,
-24, -11,   7,  26, 24, 35,  -8, -20,
-36, -26, -12,  -1,  9, -7,   6, -23,
-45, -25, -16, -17,  3,  0,  -5, -33,
-44, -16, -20,  -9, -1, 11,  -6, -71,
-19, -13,   1,  17, 16,  7, -37, -26,
        };
        return arr[pieceIndex];
    }
    case PieceGeneric::Queen:
    {
        constexpr std::array<float, 64> arr = {
-28,   0,  29,  12,  59,  44,  43,  45,
-24, -39,  -5,   1, -16,  57,  28,  54,
-13, -17,   7,   8,  29,  56,  47,  57,
-27, -27, -16, -16,  -1,  17,  -2,   1,
 -9, -26,  -9, -10,  -2,  -4,   3,  -3,
-14,   2, -11,  -2,  -5,   2,  14,   5,
-35,  -8,  11,   2,   8,  15,  -3,   1,
 -1, -18,  -9,  10, -15, -25, -31, -50,
        };
        return arr[pieceIndex];
    }
    case PieceGeneric::King:
    {
        constexpr std::array<float, 64> arr = {
-65,  23,  16, -15, -56, -34,   2,  13,
 29,  -1, -20,  -7,  -8,  -4, -38, -29,
 -9,  24,   2, -16, -20,   6,  22, -22,
-17, -20, -12, -27, -30, -25, -14, -36,
-49,  -1, -27, -39, -46, -44, -33, -51,
-14, -14, -22, -46, -44, -30, -15, -27,
  1,   7,  -8, -64, -43, -16,   9,   8,
-15,  36,  12, -54,   8, -28,  24,  14,
        };
        return arr[pieceIndex];
    }
    default:
        std::unreachable();
    }
}
//constexpr float priceAdjustment(Piece p, i8 column, i8 row)
//{
//    return priceAdjustment(toGenericPiece(p), column, row);
//}
constexpr float priceAdjustmentPov(Piece p, i8 column, i8 row)
{
    if (static_cast<i8>(p) < 0)//case(PlayerSide::BLACK):
    {
        return priceAdjustment((PieceGeneric)(-p), toIndex(column, row));
    }
    else//case(PlayerSide::WHITE):
    {
        return priceAdjustment((PieceGeneric)p, toIndex(column, 7 - row));
    }
}

constexpr float pricePiece(PieceGeneric p)
{
    switch (p)
    {
    case PieceGeneric::Nothing:
        return 0;
    case PieceGeneric::Pawn:
        return 82;
    case PieceGeneric::Knight:
        return 337;
    case PieceGeneric::Bishop:
        return 365;
    case PieceGeneric::Rook:
        return 477;
    case PieceGeneric::Queen:
        return 1025;
    case PieceGeneric::King:
        return 20000;
    default:
        std::unreachable();
    }
    std::unreachable();
}

constexpr float pricePiece(Piece p)
{
    return pricePiece(toGenericPiece(p));
}

constexpr float priceAbsolute(Piece p, i8 column, i8 row)
{
    auto res = pricePiece(p) + priceAdjustmentPov(p, column, row);
    AssertAssume(res >= 0);
    return res;
}
constexpr float priceRelative(Piece p, i8 column, i8 row)
{
    return priceAbsolute(p, column, row) * pieceColor(p);
}

std::ostream& printPiece(Piece p, std::ostream& os) {
    return os << symbolA(p);
}
std::wostream& printPiece(Piece p, std::wostream& os) {
    return os << symbolW(p);
}

constexpr i8 promoteRow(PlayerSide s) {
    switch (s)
    {
    case PlayerSide::WHITE:
        return 7;
    case PlayerSide::BLACK:
        return 0;
    default:
        std::unreachable();
    }
}

constexpr i8 initialRow(Piece p) {
    switch (p)
    {
    case Piece::PawnWhite:
        return 1;
    case Piece::KnightWhite:
    case Piece::BishopWhite:
    case Piece::RookWhite:
    case Piece::QueenWhite:
    case Piece::KingWhite:
        return 0;
    case Piece::PawnBlack:
        return 6;
    case Piece::KnightBlack:
    case Piece::BishopBlack:
    case Piece::RookBlack:
    case Piece::QueenBlack:
    case Piece::KingBlack:
        return 7;
    default:
        std::unreachable();
    }
}

constexpr std::array<Piece, 4> availablePromotes(Piece p)
{
    AssertAssume(toGenericPiece(p) == PieceGeneric::Pawn);

    constexpr std::array<std::array<Piece, 4>, 2> evolveLastRow{ {{ Piece::QueenBlack, Piece::RookBlack, Piece::BishopBlack, Piece::KnightBlack } , { Piece::QueenWhite, Piece::RookWhite, Piece::BishopWhite, Piece::KnightWhite }} };

    static_assert(index(PlayerSide::BLACK) == 0 && index(PlayerSide::WHITE) == 1); // We are assuming this in the array above

    static_assert((i8)Piece::PawnWhite == 1 && (i8)Piece::PawnBlack == -1); // With the hack that we are doing in the next step, we need the pawns to be 1 and -1.

    return evolveLastRow[index((PlayerSide)p)];
}

/*
    switch (p)
    {
    case Nothing: {
        return
    } break;
    case Pawn: {

    } break;
    case Knight: {

    } break;
    case Bishop: {

    } break;
    case Rook: {

    } break;
    case Queen: {

    } break;
    case King: {

    } break;
    default:
        std::unreachable();
    }
    std::unreachable();*/

class GameState {
    //Piece* board[64];
public:
    std::array<Piece, 64> board;
    i32 repeatableMoves;
    PlayerSide playerOnMove;
    std::array<std::array<bool, 2>, 2> canCastle;

    auto operator<=>(const GameState&) const noexcept = default;

    GameState(const GameState& copy) noexcept = default;

    GameState(GameState&& move) noexcept = default;

    GameState& operator=(const GameState& copy) noexcept = default;

    GameState& operator=(GameState&& move) noexcept = default;

    GameState() : board{ Piece::Nothing }
    {
    }

    constexpr GameState(std::array<Piece, 64> pieces, i32 repeatableMoves, PlayerSide playerOnMove, std::array<std::array<bool, 2>, 2> canCastle):board(pieces), repeatableMoves(repeatableMoves),playerOnMove(playerOnMove),canCastle(canCastle)
    {}



    constexpr std::array<i8, 128> piecesCount() const
    {
        std::array<i8, 128> res { 0 };
        for (const auto& i : board)
        {
            if (i != Piece::Nothing)
                ++res[symbolA(i)];
        }
        return res;
    }

    constexpr float priceInLocation(i8 column, i8 row, PlayerSide playerColor) const
    {
        if (column < 0 || column > 7 || row < 0 || row > 7)
            return -std::numeric_limits<float>::infinity();

        auto piece = pieceAt(column,row);

        AssertAssume(playerColor == PlayerSide::BLACK || playerColor == PlayerSide::WHITE);

        if (piece == Piece::Nothing)
            return 0;
        else
            return priceRelative(piece, column, row) * (-playerColor);
    }

    constexpr const Piece& pieceAt(i8 column, i8 row) const
    {
        AssertAssume (!(column < 0 || column > 7 || row < 0 || row > 7));
        return board[column + (row * 8)];
    }
    constexpr Piece& pieceAt(i8 column, i8 row)
    {
        AssertAssume(!(column < 0 || column > 7 || row < 0 || row > 7));
        return board[column + (row * 8)];
    }

    constexpr const Piece& pieceAt(char column, char row) const
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            throw std::exception("Invalid coordinates");
        else
            return pieceAt((i8)(column - 'a'), (i8)(row - '1'));
    }
    constexpr Piece& pieceAt(char column, char row)
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            throw std::exception("Invalid coordinates");
        else
            return pieceAt((i8)(column - 'a'), (i8)(row - '1'));
    }

    constexpr void movePiece(char columnFrom, char rowFrom, char columnTo, char rowTo)
    {
        auto& from = pieceAt(columnFrom, rowFrom);
        auto& to = pieceAt(columnTo, rowTo);

        if (from == Piece::Nothing)
            debugOut << "ERROR! Moving an empty field!" << std::endl;
            //throw std::exception("Trying to move an empty field");

        to = from;
        from = Piece::Nothing;
    }

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
        case PlayerSide::BLACK:
        {
            for (i8 i = 0; i < 8; ++i) {
                os << i + 1 << ' ';
                for (i8 j = 7; j >= 0; --j) {
                    os << '|';

                    printPiece(board[j + i * 8], os);
                }
                os << '|' << nl;
            }
            os << "   h g f e d c b a";
        } break;
        case PlayerSide::WHITE:
        {
            for (i8 i = 7; i >= 0; --i) {
                os << i + 1 << ' ';
                for (i8 j = 0; j < 8; ++j) {
                    os << '|';
                    //wcout<<((((j+i*8)%2)==0)?"\033[40m":"\033[43m");
                    printPiece(board[j + i * 8], os);
                }
                os << '|' << nl;
            }
            os << "   a b c d e f g h";
        } break;
        default:
            std::unreachable();
        }

        os << nl << std::flush;
    }


    moveNotation findDiff(const GameState& old) const
    {
        std::array<char, 6> res = { 0 };

        Piece oldPiece = Piece::Nothing;

        for (size_t i = 0; i < board.size(); ++i)
        {
            if (board[i] == Piece::Nothing && old.board[i] != Piece::Nothing)
            {
                oldPiece = old.board[i];

                res[0] = i % 8 + 'a';
                res[1] = i / 8 + '1';

                if(toGenericPiece(old.board[i]) == PieceGeneric::King)//castling hack
                    break;
            }
        }
        if (oldPiece == Piece::Nothing)
            throw std::exception("No change made");
        for (size_t i = 0; i < board.size(); ++i)
        {
            if (board[i] != Piece::Nothing && board[i] != old.board[i])
            {
                res[2] = i % 8 + 'a';
                res[3] = i / 8 + '1';

                if (toGenericPiece(oldPiece) == PieceGeneric::Pawn && (res[3] == '1' || res[3] == '8'))
                {
                    res[4] = tolower(symbolA(board[i]));
                }

                if (toGenericPiece(board[i]) == PieceGeneric::King)//castling hack
                    break;
            }
        }
        return moveNotation(res.data());
    }
    
    constexpr float balance() const {
        double res = 0;
        for (i8 i = 0; i < 64; ++i) {
            if (board[i] == Piece::Nothing) [[likely]]//Just optimization
                continue;
            if (toGenericPiece(board[i]) == PieceGeneric::King) [[unlikely]]//To avoid floating point overflow
                continue;
            res += priceRelative(board[i], i % 8, i / 8);
        }
        return res;
    }

    
    constexpr i8 countPiecesMin() const
    {
        std::array<i8, 2> counters{ 0 };
        for (const auto& i : board)
        {
            if (i != Piece::Nothing)
                ++counters[index(pieceColor(i))];
        }
        return std::min(counters[0], counters[1]);
    }

    constexpr static GameState startingPosition()
    {
        return GameState(
            {
                Piece::RookWhite, Piece::KnightWhite, Piece::BishopWhite, Piece::QueenWhite, Piece::KingWhite, Piece::BishopWhite, Piece::KnightWhite, Piece::RookWhite,
                Piece::PawnWhite, Piece::PawnWhite, Piece::PawnWhite, Piece::PawnWhite, Piece::PawnWhite, Piece::PawnWhite, Piece::PawnWhite, Piece::PawnWhite,
                Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing,
                Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing,
                Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing,
                Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing, Piece::Nothing,
                Piece::PawnBlack, Piece::PawnBlack, Piece::PawnBlack, Piece::PawnBlack, Piece::PawnBlack, Piece::PawnBlack, Piece::PawnBlack, Piece::PawnBlack,
                Piece::RookBlack, Piece::KnightBlack, Piece::BishopBlack, Piece::QueenBlack, Piece::KingBlack, Piece::BishopBlack, Piece::KnightBlack, Piece::RookBlack
            },
            0,
            PlayerSide::WHITE,
            { { { true, true },
            { true, true } } }
        );
    }
};


struct BoardHasher
{
    std::size_t operator()(const GameState& s) const noexcept
    {
        size_t* toHash = (size_t*)s.board.data();
        constexpr size_t sizeToHash = 64 / sizeof(size_t);

        size_t res = *toHash;
        for (size_t i = 1; i < sizeToHash; ++i)
        {
            res ^= toHash[i];
        }

        return res;
    }
};

static stack_vector<std::pair<GameState, float>, maxMoves> firstPositions;

template <bool saveToVector = false>
struct Variation {
    size_t nodes = 0;

    duration_t time = duration_t(0);
    float bestFoundValue;
    float startingValue;

    GameState board;

    bool pruned = false;

    i8 variationDepth;

    PlayerSide firstMoveOnMove;
    moveNotation firstMoveNotation;

    //std::unordered_map<GameState, float, BoardHasher> transpositions;


    Variation() noexcept = default;
    Variation(const Variation& copy) noexcept = default;//:researchedBoard(copy.researchedBoard),bestFoundValue(copy.bestFoundValue),pieceTakenValue(copy.pieceTakenValue){}
    Variation(Variation&& toMove) noexcept = default;// :researchedBoard(move(toMove.researchedBoard)), bestFoundValue(toMove.bestFoundValue), pieceTakenValue(toMove.pieceTakenValue) {}


    Variation& operator=(Variation&& toMove) noexcept = default;

    Variation& operator=(const Variation& copy) noexcept = default;

    friend bool operator<(const Variation& l, const Variation& r)
    {
        return l.bestFoundValue < r.bestFoundValue;
    }
    friend bool operator>(const Variation& l, const Variation& r)
    {
        return l.bestFoundValue > r.bestFoundValue;
    }

    //Variation(GameState researchedBoard, double bestFoundValue, double startingValue):researchedBoard(move(researchedBoard)),bestFoundValue(bestFoundValue), startingValue(startingValue) {}
    //Variation(GameState board, float startingValue) :board(std::move(board)), bestFoundValue(startingValue), startingValue(startingValue) {}
    Variation(GameState board, float bestFoundValue, float startingValue, i8 variationDepth, PlayerSide firstMoveOnMove, moveNotation firstMoveNotation) :board(std::move(board)), bestFoundValue(bestFoundValue), startingValue(startingValue), variationDepth(variationDepth), firstMoveOnMove(firstMoveOnMove), firstMoveNotation(firstMoveNotation){}




    bool canTakeKing(PlayerSide onMove)
    {
        float alpha, beta;
        alpha = -pricePiece(PieceGeneric::King);
        beta = pricePiece(PieceGeneric::King);

        TempSwap playerBackup(board.playerOnMove, onMove);
        //TempSwap vectorBackup(saveToVector, false);

        Variation<false>* thisHack = reinterpret_cast<Variation<false> *>(this);//TODO prasarna

        TempSwap backupCastling(board.canCastle, { {{ false,false }, { false,false }} });

        for (i8 i = 0; i < board.board.size(); ++i) {
            Piece found = board.board[i];

            if (found == Piece::Nothing)
                continue;
            if (pieceColor(found) == onMove)
            {
                auto foundVal = thisHack->bestMoveWithThisPieceScore((i % 8), (i / 8), 0, alpha, beta, 0);

                if (foundVal * onMove == pricePiece(PieceGeneric::King)) [[unlikely]]//Je možné vzít krále, hra skončila
                    {
                        return true;
                    }

            }
        }
        return false;
    }

    //bool canSquareBeTakenBy(i8 column, i8 row, PlayerSide attacker)
    //{
    //    i32 totalMoves = 0;
    //    constexpr float valueSoFar = 0;
    //    float alpha, beta;
    //    alpha = -std::numeric_limits<float>::infinity();
    //    beta = std::numeric_limits<float>::infinity();
    //    const Piece* backup = pieceAt(column, row);
    //    //if (backup == nullptr)
    //    //    return false;
    //    auto backupPlayer = playerOnMove;
    //    playerOnMove = attacker;
    //    
    //    switch (attacker)
    //    {
    //    case WHITE:
    //        pieceAt(column, row) = &baitBlack;
    //        break;
    //    case BLACK:
    //        pieceAt(column, row) = &baitWhite;
    //        break;
    //    default:
    //        std::unreachable();
    //    }
    //    
    //    for (i8 i = 0; i < board.size(); ++i) {
    //        const Piece* found = board[i];
    //        if (found == nullptr)
    //            continue;
    //        if (found->pieceColor() == attacker)
    //        {
    //            auto foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, valueSoFar);
    //            debugOut << foundVal << std::endl;
    //            if (foundVal * attacker == baitWhite.pricePiece())//Bait taken
    //            {
    //                pieceAt(column, row) = backup;
    //                playerOnMove = backupPlayer;
    //                return true;
    //            }
    //        }
    //    }
    //    pieceAt(column, row) = backup;
    //    playerOnMove = backupPlayer;
    //    return false;
    //}

    bool canMove(PlayerSide onMove)
    {
        float alpha, beta;
        alpha = -pricePiece(PieceGeneric::King);
        beta = pricePiece(PieceGeneric::King);

        for (i8 i = 0; i < board.board.size(); ++i) {
            Piece found = board.board[i];

            if (found == Piece::Nothing)
                continue;
            if (pieceColor(found) == onMove)
            {
                auto foundVal = bestMoveWithThisPieceScore((i % 8), (i / 8), 0, alpha, beta, 0);

                if (foundVal != std::numeric_limits<float>::infinity() * (-1) * onMove)
                    return true;
            }
        }
        return false;
    }

    bool isValidSetup()
    {
        //TempSwap saveVectorBackup(saveToVector, false);
        
        //bool backup = saveToVector;
        //saveToVector = false;
        //Both kings must be on the researchedBoard exactly once
        auto count = board.piecesCount();
        if (count['k'] != 1 || count['K'] != 1)
            return false;

        //print(debugOut);

        //if (!canMove(opposideSide(playerOnMove)))
          //  return false;
        if (canTakeKing(board.playerOnMove))
            return false;

        //saveToVector = backup;

        //TODO the rest
        return true;
    }
    //double positionScoreHeuristic() {
    //    double res = 0;
    //    for (i8 i = 0; i < 64; ++i) {
    //        if (board[i] != nullptr)
    //        {
    //            float alpha, beta;
    //            alpha = -std::numeric_limits<float>::max();
    //            beta = std::numeric_limits<float>::max();
    //            i32 totalMoves = 0;
    //            double totalValues = 0;
    //            board[i]->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, totalMoves, totalValues, 0,true)
    //            res += (((double)totalMoves)/2.0) * board[i]->pieceColor();
    //            if(totalMoves>0)
    //                res += std::min(std::max(totalValues,-100.0),100.0);
    //        }
    //    }
    //    return res / 10000.0;
    //}


    //template <bool saveToVector>
    float bestMoveScore(i8 depth, float valueSoFar, float alpha, float beta)
    {
        AssertAssume(board.playerOnMove == PlayerSide::WHITE || board.playerOnMove == PlayerSide::BLACK);

        //if(depth>3)
        //{
        //    auto found = transpositions.find(*this);
        //    if (found != transpositions.end())
        //        return found->second;
        //}

        //{
        //    debugOut << "Found it" << nl << std::flush;
        //    print(debugOut);
        //}


        float bestValue = -std::numeric_limits<float>::infinity() * board.playerOnMove;

        if (criticalTimeDepleted) [[unlikely]]
            return bestValue;

        if (depth > depthToStopOrderingPieces) [[unlikely]]
        {
            stack_vector<std::pair<float, i8>, 16> possiblePiecesToMove;

            for (i8 i = 0; i < 64; ++i) {
                float alphaTmp = alpha;
                float betaTmp = beta;
                const Piece found = board.board[i];

                if (found == Piece::Nothing)
                    continue;
                if (pieceColor(found) == board.playerOnMove)
                    possiblePiecesToMove.emplace_back(bestMoveWithThisPieceScore(i % 8, i / 8, -1, alphaTmp, betaTmp, valueSoFar), i);
            }

            switch (board.playerOnMove)
            {
            case PlayerSide::WHITE: {
                std::sort(possiblePiecesToMove.begin(), possiblePiecesToMove.end(), [](auto& left, auto& right) {return left.first > right.first; });
            } break;
            case PlayerSide::BLACK: {
                std::sort(possiblePiecesToMove.begin(), possiblePiecesToMove.end(), [](auto& left, auto& right) {return left.first < right.first; });
            } break;
            default:
                std::unreachable();
            }

            for (const auto& move : possiblePiecesToMove) {
                i8 i = move.second;
                const Piece found = board.board[i];
                float foundVal;
                if (depth > depthToStopOrderingMoves) [[unlikely]]
                {
                    foundVal = bestMoveWithThisPieceScoreOrdered((i % 8), (i / 8), depth - 1, alpha, beta, valueSoFar);
                }
                else
                {
                    foundVal = bestMoveWithThisPieceScore((i % 8), (i / 8), depth - 1, alpha, beta, valueSoFar);
                }

                //assert(variationDepth == depthW);

                if (firstLevelPruning && depth == variationDepth) [[unlikely]]
                {
                    //std::osyncstream(debugOut) << "alpha: " << alpha << ", beta: " << beta << std::endl;
                    switch (board.playerOnMove)
                    {
                    case PlayerSide::WHITE: {
                        if (beta != alphaOrBeta)
                        {
                            beta = alphaOrBeta;
                            pruned = true;
                        }
                    } break;
                    case PlayerSide::BLACK: {
                        if (alpha != alphaOrBeta)
                        {
                            alpha = alphaOrBeta;
                            pruned = true;
                        }
                    } break;
                    default:
                        std::unreachable();
                    }
                    //std::osyncstream(debugOut) << "alpha: " << alpha << ", beta: " << beta << std::endl;
                }


                if (foundVal * board.playerOnMove > bestValue * board.playerOnMove)
                {
                    bestValue = foundVal;
                }
                if (foundVal * board.playerOnMove == pricePiece(PieceGeneric::King))//Je možné vzít krále, hra skončila
                {
                    //wcout << endl;
                    //print();
                    //break;
                    return foundVal;//*depth;
                }
                if (beta <= alpha && bestValue != -std::numeric_limits<float>::infinity() * board.playerOnMove)
                {
                    break;
                    //depthToPieces = 0;
                }
            }
        }
        else [[likely]]
        {
            for (i8 i = 0; i < 64; ++i) {
                const Piece found = board.board[i];

                if (found == Piece::Nothing)
                    continue;
                if (pieceColor(found) == board.playerOnMove)
                {
                    auto foundVal = bestMoveWithThisPieceScore((i % 8), (i / 8), depth - 1, alpha, beta, valueSoFar);

                    if (foundVal * board.playerOnMove > bestValue * board.playerOnMove) {
                        bestValue = foundVal;
                    }
                    if (foundVal * board.playerOnMove == pricePiece(PieceGeneric::King))//Je možné vzít krále, hra skončila
                    {
                        //wcout << endl;
                        //print();
                        //break;
                        return foundVal;//*depth;
                    }
                    if (beta <= alpha && bestValue != -std::numeric_limits<float>::infinity() * board.playerOnMove)
                    {
                        break;
                        //depthToPieces = 0;
                    }
                }
            }
        }

        //if (saveToVector) [[unlikely]]
            //return -std::numeric_limits<float>::max();


        if (bestValue == (float)(std::numeric_limits<float>::infinity() * board.playerOnMove * (-1))) [[unlikely]]//Nemůžu udělat žádný legitimní tah (pat nebo mat)
        {
            //TempSwap saveToVectorBackup(saveToVector, false);

            if (canTakeKing(oppositeSide(board.playerOnMove)))//if (round((bestMoveScore(1, onMove * (-1),valueSoFar, alpha, beta) * playerOnMove * (-1))/100) == kingPrice/100)//Soupeř je v situaci, kdy mi může vzít krále (ohrožuje ho)
            {
                bestValue = -matePrice * board.playerOnMove;//Dostanu mat, co nejnižší skóre
            }
            else
            {//Dostal bych pat, ten je vždycky lepší než dostat mat, ale chci ho docílit jen když prohrávám
                bestValue = 0;//When we return 0, stalemate is preffered only if losing (0 is better than negative)
                //if (valueSoFar * playerOnMove > 0)
                //{
                //    //debugOut << "Stalemate not wanted for " << (unsigned)onMove << std::endl;
                //    bestValue = -stalePrice * playerOnMove;//Vedu, nechci dostat pat
                //}
                //else
                //{
                //    //debugOut << "Stalemate wanted for "<< (unsigned)onMove << std::endl;
                //    bestValue = stalePrice * playerOnMove;//Prohrávám, dostat pat beru jako super tah
                //}  
            }
            bestValue *= depth;//To get shit done quickly
        }

        //if (depth > 3)
        //    transpositions.emplace(*this, bestValue);
        return bestValue;
    }


    auto tryPiece(i8 column, i8 row, Piece p, i8 depth, float alpha, float beta, float valueSoFar)
    {
        TempSwap pieceBackup(board.pieceAt(column, row), p);
        board.playerOnMove = oppositeSide(board.playerOnMove);
        auto tmp = bestMoveScore(depth, valueSoFar, alpha, beta);
        board.playerOnMove = oppositeSide(board.playerOnMove);
        return tmp;
    }

    void placePieceAt(Piece p, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, float valueSoFar, float priceTaken)
    {
        ++nodes;
        if constexpr (saveToVector)
        {
            if (depth == 0)
            {
                TempSwap backup(board.pieceAt(column, row), p);
                board.playerOnMove = oppositeSide(board.playerOnMove);
                if (isValidSetup())
                {
                    //float pieceTakenValue = valueSoFar + priceAbsolute * pieceColor();
                    //board.print();
                    float balance = board.balance();
                    firstPositions.emplace_back(board, balance);
                }
                board.playerOnMove = oppositeSide(board.playerOnMove);
                return;
            }
        }


        //if (doNotContinue) [[unlikely]]
        //    return;

        if (priceTaken >= pricePiece(PieceGeneric::King) - 128) [[unlikely]]//Possible to take king (probaly check?)
        {
            //doNotContinue = true;
            bestValue = pricePiece(PieceGeneric::King) * board.playerOnMove;
            //totalMoves++;
            //return;
        }
        else [[likely]]
        {
            float valueGained = priceTaken + priceAdjustmentPov(p, column, row); //We are entering new position with this piece

            //assert(variationDepth == depthW);
            const auto wholeMovesFromRoot = ((variationDepth - depth) / 2);//Kolikaty tah od initial pozice (od 0), ne pultah
            valueGained *= (1 - (wholeMovesFromRoot * 0.0001));//10000.0);

            valueSoFar += valueGained * board.playerOnMove;//Add our gained value to the score

            float foundVal;
            if (depth > 0)
            {
                foundVal = tryPiece(column, row, p, depth, alpha, beta, valueSoFar);

                if ((foundVal * board.playerOnMove * (-1)) == pricePiece(PieceGeneric::King))//V dalším tahu bych přišel o krále, není to legitimní tah
                    return;
            }
            else//leaf node of the search tree
                foundVal = valueSoFar;

            if (foundVal * board.playerOnMove > bestValue * board.playerOnMove)
                bestValue = foundVal;
        }

        switch (board.playerOnMove)
        {
        case(PlayerSide::WHITE): {
            alpha = std::max(alpha, bestValue);//bily maximalizuje hodnotu
        } break;
        case(PlayerSide::BLACK): {
            beta = std::min(beta, bestValue);
        } break;
        default:
            std::unreachable();
        }

        //doNotContinue |= (beta <= alpha);
    }


    template <typename F>
    bool tryPlacingPieceAt(Piece p, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, float valueSoFar, F condition)
    {
        if (beta <= alpha && bestValue != -std::numeric_limits<float>::infinity() * board.playerOnMove)
            return false;

        float price = board.priceInLocation(column, row, board.playerOnMove);

        if (condition(price, 0))
        {
            placePieceAt(p, column, row, depth, alpha, beta, bestValue, valueSoFar, price);
            return price == 0;
        }
        else
            return false;
    }

    auto tryPlacingPieceAt(Piece p, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, float valueSoFar)
    {
        return tryPlacingPieceAt(p, column, row, depth, alpha, beta, bestValue, valueSoFar, MOVE_PIECE_FREE_CAPTURE);
    }

    template <typename T>
    bool addMoveToList(Piece p, i8 column, i8 row, float alpha, float beta, T& possibleMoves)
    {
        float bestValue = -std::numeric_limits<float>::infinity() * board.playerOnMove;
        bool fieldWasFree = tryPlacingPieceAt(p, column, row, 0, alpha, beta, bestValue, 0);

        if (bestValue != -std::numeric_limits<float>::infinity() * board.playerOnMove)
            possibleMoves.emplace_back(bestValue, std::make_pair(column, row));

        return fieldWasFree;
    }

    template <i8 rookColumn, i8 newRookColumn>
    void tryCastling(Piece p, i8 row, /*i8 kingColumn, i8 rookColumn, i8 newRookColumn,*/ bool& canICastleLeft, bool& canICastleRight, float& bestValue, i8 depth, float& alpha, float& beta, float valueSoFar)
    {
        AssertAssume(row == 0 || row == 7);

        constexpr i8 kingColumn = 4;
        constexpr i8 sign = rookColumn < kingColumn ? 1 : -1;
        constexpr i8 newKingColumn = kingColumn - 2 * sign;
        /*Castling is permitted only if
          - neither the king nor the rook has previously moved;
          - the squares between the king and the rook are vacant;
          - the king does not leave, cross over, or finish on a square attacked by an enemy piece.
        */

        for (i8 i = rookColumn + sign; i != kingColumn; i += sign)
        {
            if (board.pieceAt(i, row) != Piece::Nothing) [[likely]]//The path is not vacant
                return;
        }

        auto pieceInCorner = board.pieceAt(rookColumn, row);
        if (toGenericPiece(pieceInCorner) != PieceGeneric::Rook)//The piece in the corner is not a rook or is vacant
            return;

        //assert(variationDepth == depthW);

        if (variationDepth - depth <= castlingMaxDepth) [[unlikely]]//To optimize time, we do not check whether the path is attacked, unless the move is immediate. May produce bad predictions, but only for small number of cases.
        {
            for (i8 i = kingColumn; i != newKingColumn; i -= sign)//Do not check the last field (where the king should be placed), it will be checked later anyway
            {
                TempSwap fieldSwap(board.pieceAt(i, row), p);
                if (canTakeKing(oppositeSide(pieceColor(p)))) [[unlikely]]//The path is attacked by enemy
                {
                    return;
                }
            }
        }

        //Now we can be sure we can do the castling
        valueSoFar -= priceAdjustmentPov(pieceInCorner, rookColumn, row);//Remove the position score of the rook, it is leaving
        valueSoFar += priceAdjustmentPov(pieceInCorner, newRookColumn, row);//Add the score of the rook on the next position

        //Castling is not allowed from this point onwards
        TempSwap castleLeftBackup(canICastleLeft, false);
        TempSwap castleRightBackup(canICastleRight, false);

        //Do the actual piece movement
        TempSwap rookBackup(board.pieceAt(rookColumn, row), Piece::Nothing);
        TempSwap newRookBackup(board.pieceAt(newRookColumn, row), pieceInCorner);
        tryPlacingPieceAt(p, newKingColumn, row, depth - 1, alpha, beta, bestValue, valueSoFar);
        //State will be restored when calling destructors
    }

    float bestMoveWithThisPieceScore(i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar)
    {
        float bestValue = -std::numeric_limits<float>::infinity() * board.playerOnMove;

        Piece p = board.pieceAt(column, row);
        PieceGeneric piece = toGenericPiece(p);

        AssertAssume(pieceColor(p) == board.playerOnMove);

        board.pieceAt(column, row) = Piece::Nothing;
        valueSoFar -= priceAdjustmentPov(p, column, row) * board.playerOnMove;//We are leaving our current position
        //board.playerOnMove = oppositeSide(board.playerOnMove);

        switch (piece)
        {
        case PieceGeneric::Nothing:
            break;
        case PieceGeneric::Pawn:
        {
            if (row + playerDirection(board.playerOnMove) == promoteRow(board.playerOnMove)) [[unlikely]]
            {
                const auto& availableOptions = availablePromotes(p);
                for (const auto& evolveOption : availableOptions) {
                    float valueDifferenceNextMove = (pricePiece(evolveOption) - pricePiece(piece)) * board.playerOnMove;//Increase in material when the pawn promotes
                    float valueSoFarEvolved = valueSoFar + valueDifferenceNextMove;

                    //Capture diagonally
                    tryPlacingPieceAt(evolveOption, column - 1, row + playerDirection(board.playerOnMove), depth, alpha, beta, bestValue, valueSoFarEvolved, MOVE_PIECE_CAPTURE_ONLY);
                    tryPlacingPieceAt(evolveOption, column + 1, row + playerDirection(board.playerOnMove), depth, alpha, beta, bestValue, valueSoFarEvolved, MOVE_PIECE_CAPTURE_ONLY);

                    //Go forward
                    tryPlacingPieceAt(evolveOption, column, row + playerDirection(board.playerOnMove), depth, alpha, beta, bestValue, valueSoFarEvolved, MOVE_PIECE_FREE_ONLY);
                }
            }
            else [[likely]]
            {
                //Capture diagonally
                tryPlacingPieceAt(p, column - 1, row + playerDirection(board.playerOnMove), depth, alpha, beta, bestValue, valueSoFar, MOVE_PIECE_CAPTURE_ONLY);
                tryPlacingPieceAt(p, column + 1, row + playerDirection(board.playerOnMove), depth, alpha, beta, bestValue, valueSoFar, MOVE_PIECE_CAPTURE_ONLY);

                //Go forward

                //First, try two fields forward (if possible) since it is usually the better option
                if (row == initialRow(p) && board.pieceAt(column, row + playerDirection(board.playerOnMove)) == Piece::Nothing)
                    tryPlacingPieceAt(p, column, row + playerDirection(board.playerOnMove) * 2, depth, alpha, beta, bestValue, valueSoFar, MOVE_PIECE_FREE_ONLY);

                //Try one field forward
                tryPlacingPieceAt(p, column, row + playerDirection(board.playerOnMove), depth, alpha, beta, bestValue, valueSoFar, MOVE_PIECE_FREE_ONLY);
            }

        } break;
        case PieceGeneric::Knight:
        {
            tryPlacingPieceAt(p, column + 1, row + 2, depth, alpha, beta, bestValue, valueSoFar);
            tryPlacingPieceAt(p, column + 1, row - 2, depth, alpha, beta, bestValue, valueSoFar);
            tryPlacingPieceAt(p, column + 2, row + 1, depth, alpha, beta, bestValue, valueSoFar);
            tryPlacingPieceAt(p, column + 2, row - 1, depth, alpha, beta, bestValue, valueSoFar);
            tryPlacingPieceAt(p, column - 1, row + 2, depth, alpha, beta, bestValue, valueSoFar);
            tryPlacingPieceAt(p, column - 1, row - 2, depth, alpha, beta, bestValue, valueSoFar);
            tryPlacingPieceAt(p, column - 2, row + 1, depth, alpha, beta, bestValue, valueSoFar);
            tryPlacingPieceAt(p, column - 2, row - 1, depth, alpha, beta, bestValue, valueSoFar);

        } break;
        case PieceGeneric::Bishop:
        {
            for (i8 i = 1; tryPlacingPieceAt(p, column + i, row + i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column + i, row - i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column - i, row + i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column - i, row - i, depth, alpha, beta, bestValue, valueSoFar); ++i);
        } break;
        case PieceGeneric::Rook:
        {
            std::optional<TempSwap<bool>> castleBackup;

            assert(column <= 7 && column >= 0);

            if (initialRow(p) == row && column % 7 == 0)
                castleBackup.emplace(board.canCastle[column / 7][index(board.playerOnMove)], false);

            for (i8 i = 1; tryPlacingPieceAt(p, column, row + i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column, row - i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column + i, row, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column - i, row, depth, alpha, beta, bestValue, valueSoFar); ++i);

        } break;
        case PieceGeneric::Queen:
        {
            for (i8 i = 1; tryPlacingPieceAt(p, column + i, row + i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column + i, row - i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column - i, row + i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column - i, row - i, depth, alpha, beta, bestValue, valueSoFar); ++i);

            for (i8 i = 1; tryPlacingPieceAt(p, column, row + i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column, row - i, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column + i, row, depth, alpha, beta, bestValue, valueSoFar); ++i);
            for (i8 i = 1; tryPlacingPieceAt(p, column - i, row, depth, alpha, beta, bestValue, valueSoFar); ++i);

        } break;
        case PieceGeneric::King:
        {
            bool& canICastleLeft = board.canCastle[0][index(board.playerOnMove)];
            bool& canICastleRight = board.canCastle[1][index(board.playerOnMove)];


#ifndef CASTLING_DISABLED
            if (canICastleLeft)//Neither has moved
            {
                AssertAssume(column == 4);//King has to be in initial position
                AssertAssume(row == 0 || row == 7);
                tryCastling<0, 3>(p, row, canICastleLeft, canICastleRight, bestValue, depth + 1, alpha, beta, valueSoFar);
            }
            if (canICastleRight)//Neither has moved
            {
                AssertAssume(column == 4);//King has to be in initial position
                AssertAssume(row == 0 || row == 7);
                tryCastling<7, 5>(p, row, canICastleLeft, canICastleRight, bestValue, depth + 1, alpha, beta, valueSoFar);
            }
#endif

            //Classic king movement
            {
                TempSwap castleLeftBackup(canICastleLeft, false);
                TempSwap castleRightBackup(canICastleRight, false);

                tryPlacingPieceAt(p, column + 1, row + 1, depth, alpha, beta, bestValue, valueSoFar);
                tryPlacingPieceAt(p, column + 1, row,     depth, alpha, beta, bestValue, valueSoFar);
                tryPlacingPieceAt(p, column + 1, row - 1, depth, alpha, beta, bestValue, valueSoFar);
                tryPlacingPieceAt(p, column - 1, row + 1, depth, alpha, beta, bestValue, valueSoFar);
                tryPlacingPieceAt(p, column - 1, row,     depth, alpha, beta, bestValue, valueSoFar);
                tryPlacingPieceAt(p, column - 1, row - 1, depth, alpha, beta, bestValue, valueSoFar);
                tryPlacingPieceAt(p, column    , row + 1, depth, alpha, beta, bestValue, valueSoFar);
                tryPlacingPieceAt(p, column    , row - 1, depth, alpha, beta, bestValue, valueSoFar);
            }
        } break;
        default:
            std::unreachable();
        }

        board.pieceAt(column, row) = p;

        return bestValue;
    }

    //float bestMoveWithThisPieceScore(Piece p, GameState& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNotContinue = false)
    //{
    //    return bestMoveWithThisPieceScore(toGenericPiece(p), board, column, row, depth, alpha, beta, valueSoFar, doNotContinue);
    //}

    float bestMoveWithThisPieceScoreOrdered(i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar)
    {
        Piece p = board.pieceAt(column, row);

        std::optional<TempSwap<bool>> castleBackup; // Castling backup (only if moving rooks/king)

        stack_vector<std::pair<float, std::pair<i8, i8>>, 27> possibleMoves;

        switch (toGenericPiece(p))
        {
        case PieceGeneric::Nothing:
            break;
        case PieceGeneric::Pawn:
        {
            // No need to order/sort pawn movement, there are very few options which can usually be ordered hard-coded.
            return bestMoveWithThisPieceScore(column, row, depth, alpha, beta, valueSoFar);
        } break;
        case PieceGeneric::Knight:
        {
            addMoveToList(p, column + 1, row + 2, alpha, beta, possibleMoves);
            addMoveToList(p, column + 1, row - 2, alpha, beta, possibleMoves);
            addMoveToList(p, column + 2, row + 1, alpha, beta, possibleMoves);
            addMoveToList(p, column + 2, row - 1, alpha, beta, possibleMoves);
            addMoveToList(p, column - 1, row + 2, alpha, beta, possibleMoves);
            addMoveToList(p, column - 1, row - 2, alpha, beta, possibleMoves);
            addMoveToList(p, column - 2, row + 1, alpha, beta, possibleMoves);
            addMoveToList(p, column - 2, row - 1, alpha, beta, possibleMoves);

        } break;
        case PieceGeneric::Bishop:
        {
            for (i8 i = 1; addMoveToList(p, column + i, row + i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column + i, row - i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column - i, row + i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column - i, row - i, alpha, beta, possibleMoves); ++i);
        } break;
        case PieceGeneric::Rook:
        {
            assert(column <= 7 && column >= 0);

            // Back up castling if needed
            if (initialRow(p) == row && column % 7 == 0)
                castleBackup.emplace(board.canCastle[column / 7][index(board.playerOnMove)], false);

            for (i8 i = 1; addMoveToList(p, column + i, row, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column - i, row, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column, row + i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column, row - i, alpha, beta, possibleMoves); ++i);

            // Castling will be restored only after the actual tryout, not here
        } break;
        case PieceGeneric::Queen:
        {
            for (i8 i = 1; addMoveToList(p, column + i, row + i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column + i, row - i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column - i, row + i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column - i, row - i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column + i, row, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column - i, row, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column, row + i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column, row - i, alpha, beta, possibleMoves); ++i);
        } break;
        case PieceGeneric::King:
        {
            //TODO make this piece work
            //Watch out for castling support!
            return bestMoveWithThisPieceScore(column, row, depth, alpha, beta, valueSoFar);
        } break;
        default:
            std::unreachable();
        }

        switch (board.playerOnMove)
        {
        case PlayerSide::WHITE: {
            std::sort(possibleMoves.begin(), possibleMoves.end(), [](auto& left, auto& right) {return left.first > right.first; });
        } break;
        case PlayerSide::BLACK: {
            std::sort(possibleMoves.begin(), possibleMoves.end(), [](auto& left, auto& right) {return left.first < right.first; });
        } break;
        default:
            std::unreachable();
        }

        TempSwap pieceBackup(board.pieceAt(column, row), Piece::Nothing);
        float bestValue = -std::numeric_limits<float>::infinity() * board.playerOnMove;

        valueSoFar -= priceAdjustmentPov(p, column, row) * board.playerOnMove;//We are leaving our current position

        for (const auto& i : possibleMoves)
            tryPlacingPieceAt(p, i.second.first, i.second.second, depth, alpha, beta, bestValue, valueSoFar);

        return bestValue;
    }
};





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
        o << "cp " << round(scoreCp * playerOnMove);
    return o;
}

void printLowerBound(float currentLowerBound, i8 depth)
{
    out << "info depth " << (unsigned)fullDepth << ' ';
    if (depth + 1 != fullDepth)
        out << "seldepth " << (unsigned)depth + 1 << ' ';

    printScore(out, currentLowerBound, depth + 1, oppositeSide(onMoveW));
    out << ' ';
    out << "lowerbound";
    out << nl << std::flush;
}

//std::atomic<size_t> totalNodesDepth;
size_t totalNodesAll;
std::optional<duration_t> timeForTheFirst;

auto evaluateGameMove(Variation<> localBoard)//, double alpha = -std::numeric_limits<float>::max(), double beta = std::numeric_limits<float>::max())
{
    if (localBoard.variationDepth > 0) [[likely]] //Not predetermined result - e.g. not a draw by repetition
    {
        auto timeStart = std::chrono::high_resolution_clock::now();
        //Variation localBoard(board);

        if (!firstLevelPruning)//If we want to know multiple good moves, we cannot prune using a/B at root level
        {
            localBoard.bestFoundValue = localBoard.bestMoveScore(localBoard.variationDepth, localBoard.startingValue, -pricePiece(PieceGeneric::King), pricePiece(PieceGeneric::King));
        }
        else
        {
            float localAlphaBeta = alphaOrBeta;
            if (abs(localAlphaBeta) != pricePiece(PieceGeneric::King))
                localBoard.pruned = true;

            switch (localBoard.board.playerOnMove)
            {
            case PlayerSide::BLACK: {
                localBoard.bestFoundValue = localBoard.bestMoveScore(localBoard.variationDepth, localBoard.startingValue, localAlphaBeta, pricePiece(PieceGeneric::King));
            } break;
            case PlayerSide::WHITE: {
                localBoard.bestFoundValue = localBoard.bestMoveScore(localBoard.variationDepth, localBoard.startingValue, -pricePiece(PieceGeneric::King), localAlphaBeta);
            } break;
            default:
                std::unreachable();
            }
        }

        //board.nodes = localBoard.nodes;
        localBoard.time = std::chrono::high_resolution_clock::now() - timeStart;
    }
    else
        localBoard.time = duration_t(0); //Known draw by repetition
    switch (localBoard.board.playerOnMove)
    {
    case PlayerSide::BLACK: {
        update_max(alphaOrBeta, localBoard.bestFoundValue);
    } break;
    case PlayerSide::WHITE: {
        update_min(alphaOrBeta, localBoard.bestFoundValue);
    } break;
    default:
        std::unreachable();
    }

    //totalNodesDepth += localBoard.nodes;
    return localBoard;
}

static Variation<>* bestMove;
static stack_vector<Variation<>, maxMoves>* q;
static std::atomic<size_t> qPos;

void workerFromQ(size_t threadId)//, double alpha = -std::numeric_limits<float>::max(), double beta = std::numeric_limits<float>::max())
{
    while (true)
    {
        if (optimalTimeDepleted || criticalTimeDepleted) [[unlikely]]
            break;
        size_t localPos = qPos.fetch_add(1ull, std::memory_order_relaxed);
        if (localPos >= q->size()) [[unlikely]]//Stopper
        {
            --qPos;
            break;
        }


        auto& board = (*q)[localPos];
        AssertAssume(board.board.playerOnMove == onMoveW);
        if (options.Verbosity >= 3)//[[likely]]
        {
            std::osyncstream(out)
                << "info "
                << "currmove "
                << board.firstMoveNotation << ' '
                << "currmovenumber "
                << (localPos + 1)
                << nl << std::flush;
        }
        auto res = evaluateGameMove(board);//TODO maybe move possible

        //size_t localSolvedPos = solvedPos.fetch_add(1ull, std::memory_order_relaxed);

        if (!criticalTimeDepleted) [[likely]]
        {
            board = std::move(res);

            static std::mutex m_solved;
            std::unique_lock lk(m_solved);
            //solvedMoves->push_back(std::move(res));

            if (bestMove == nullptr || board.bestFoundValue * board.board.playerOnMove < (bestMove->bestFoundValue * board.board.playerOnMove))
            {
                board.pruned = false;
                bestMove = &board;

                if (options.Verbosity >= 4)
                    printLowerBound(bestMove->bestFoundValue, board.variationDepth);
            }
        }
        else
        {
            //board.time = std::numeric_limits<duration_t>::infinity();
            //ALREADY DONE ^
        }
        //if (criticalTimeDepleted) [[unlikely]]
        //    --qPos;
    }
}

std::atomic<bool> workToDo = false;
std::atomic<bool> threadRestartWanted = false;
void on_completion() noexcept {
    workToDo = false;
}
std::optional<std::barrier<void(*)() noexcept>> barrier;

stack_vector<std::thread, maxMoves> threadWorkers;



void threadWorker(size_t threadId)
{
    while (true)
    {
        //debugOut << "thread " << threadId<<" ready for more work" << std::endl;
        workToDo.wait(false);//Wait until next work is ready
        //debugOut << "thread " << threadId <<" notified" << std::endl;
        if (threadRestartWanted) [[unlikely]]//For changing the amount of workers
            break;
        workerFromQ(threadId);//Do the actual work
        //debugOut << "thread " << threadId << " arrived and waiting" << std::endl;
        //Signal that this worker is done and wait until all of them are.
        barrier->arrive_and_wait();
    }
}

void threadRestart(size_t threadsNewCount)
{
    if (!threadWorkers.empty())
    {
        threadRestartWanted = true;
        workToDo = true;
        workToDo.notify_all();

        for (auto& i : threadWorkers)
            i.join();

        threadWorkers.clear();
    }

    workToDo = false;
    threadRestartWanted = false;

    barrier.emplace(threadsNewCount, on_completion);

    for (size_t i = 1; i < threadsNewCount; ++i)
        threadWorkers.emplace_back(threadWorker, i);
}

bool cutoffBadMoves(stack_vector<Variation<>,maxMoves>& boards, float cutoffPointRelative)
{
    float bestMoveScore = -boards[0].bestFoundValue * boards[0].firstMoveOnMove;

    float cutoffPoint = bestMoveScore - cutoffPointRelative;

    stack_vector<Variation<>, maxMoves> newBoards;

    size_t cutoffCounter = 0;

    for (size_t i = 1; i < boards.size(); ++i)
    {
        if ((-boards[i].bestFoundValue * boards[0].firstMoveOnMove) >= cutoffPoint)
            newBoards.push_back(std::move(boards[i]));
        else
            ++cutoffCounter;
    }

    debugOut << "Cutting off " << cutoffCounter << " bad moves to save time" << std::endl;

    return cutoffCounter != 0;
}




void printMoveInfo(unsigned depth, const duration_t& elapsedTotal, const duration_t& elapsedDepth, const size_t& nodesDepth, const Variation<>& move, size_t moveRank, PlayerSide pov)
{
    const auto secondsPassed = std::chrono::duration_cast<std::chrono::duration<double>>(elapsedDepth);
    out
        << "info "
        << "depth " << (unsigned)fullDepth << ' ';
    if (depth != fullDepth) [[unlikely]]
        out << "seldepth " << depth << ' ';
    out
        << "time "  << (size_t)round(elapsedTotal.count()) << ' '
        << "nodes " << totalNodesAll << ' '
        << "nps " << (size_t)round(nodesDepth / secondsPassed.count())<< ' '
        ;
    printScore(out, move.bestFoundValue, depth, pov) << ' ';
    if (move.pruned)
        out << "upperbound ";
    out
        << "multipv " << moveRank << ' '
        << "pv " << move.firstMoveNotation
        << nl;
}

static auto rd = std::random_device{};
static auto rng = std::default_random_engine{ rd() };

stack_vector<Variation<>,maxMoves> generateMoves(const GameState& board, PlayerSide bestForWhichSide, const stack_vector<std::array<Piece, 64>, 75>& playedPositions)//, i8 depth = 1
{
    alphaOrBeta = pricePiece(PieceGeneric::King) * board.playerOnMove;
    const i8 depth = 1;
    criticalTimeDepleted = false;
    onMoveW = board.playerOnMove;
    fullDepth = 1;
    //depthW = 1;
    //totalNodesDepth = 0;
    totalNodesAll = 0;
    //saveToVector = true;

    if (options.Verbosity >= 2)
        out << "info depth 1" << nl << std::flush;
    //transpositions.clear();
    Variation<true> tmp(board, 0, 0, 1, board.playerOnMove, "");
    //tmp.saveToVector = true;

    firstPositions.clear();
    tmp.bestMoveScore(1, 0, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    //totalNodesDepth = tmp.nodes;
    totalNodesAll = tmp.nodes;
    //transpositions.clear();
    stack_vector<Variation<>,maxMoves> res;// = std::move(firstPositions);
    //res.reserve(firstPositions.size());

    //Add move names
    for (auto& pos : firstPositions)
    {
        if (std::find(playedPositions.begin(), playedPositions.end(), pos.first.board) != playedPositions.end()) [[unlikely]]
        {
            res.emplace_back(pos.first, 0, 0, 0, pos.first.playerOnMove, pos.first.findDiff(board));
            debugOut << "Deja vu! Found a possible move that results in an already played position: " << res.back().firstMoveNotation <<". Assigning a value of a draw." << std::endl;
        }
        else [[likely]]
            res.emplace_back(pos.first, pos.second, pos.second, depth, pos.first.playerOnMove, pos.first.findDiff(board));
        //pos.startingValue *= bestForWhichSide;//To our POV
        //pos.bestFoundValue *= bestForWhichSide;//To our POV
        //pos.firstMoveNotation = pos.board.findDiff(board);
        //pos.variationDepth = depth;
        //pos.firstMoveOnMove = pos.board.playerOnMove;
        //pos.movePosition = pos.researchedBoard;
    }
    //firstPositions.clear();
    //if (depth > 1)
    //{
    //    auto firstMoves = std::move(res);
    //    res.clear();

    //    onMoveW = oppositeSide(board.playerOnMove);
    //    depthW = depth - 1;


    //    size_t i = 0;
    //    for (auto& pos : firstMoves)
    //    {
    //        pos.researchedBoard.bestMoveScore(depth - 1, pos.startingValue, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    //        for (; i < firstPositions.size(); ++i)
    //        {
    //            auto& move = firstPositions[i];
    //            move.firstMoveNotation = pos.firstMoveNotation;
    //            move.variationDepth = pos.variationDepth;
    //            move.firstMoveOnMove = pos.firstMoveOnMove;
    //            //move.startingValue = pos.startingValue;
    //            //move.movePosition = pos.movePosition;
    //        }
    //    }

    //    res = std::move(firstPositions);
    //    firstPositions.clear();
    //}

    //saveToVector = false;

    if (shuffle)
    {
        std::shuffle(res.begin(), res.end(), rng);
    }

    switch (bestForWhichSide)
    {
    case PlayerSide::WHITE: {
        std::sort(res.begin(), res.end(), std::greater<Variation<>>());
    } break;
    case PlayerSide::BLACK: {
        std::sort(res.begin(), res.end(), std::less<Variation<>>());
    } break;
    default:
        std::unreachable();
    }

    duration_t elapsedTotal = std::chrono::high_resolution_clock::now() - timeGlobalStarted;

    if (options.Verbosity >= 5)
    {
        for (size_t i = res.size() - 1; i != 0; --i)
            printMoveInfo(depth, elapsedTotal, elapsedTotal, tmp.nodes, res[i], i + 1, bestForWhichSide);
    }
    if (options.Verbosity >= 1)
    {
        printMoveInfo(depth, elapsedTotal, elapsedTotal, tmp.nodes, res[0], 1, bestForWhichSide);
    }

    std::cout << std::flush;

    return res;
}

duration_t findBestOnSameLevel(stack_vector<Variation<>, maxMoves>& boards, i8 depth)//, PlayerSide onMove)
{
    AssertAssume(!boards.empty());
    PlayerSide onMoveResearched = boards.front().board.playerOnMove;
    depth -= 1;// boards.front().variationDepth;
    size_t totalNodesDepth = 0;
    duration_t timeFirstBoard(0);
    //i8 previousDepth = std::numeric_limits<i8>::max();

    for (auto& i : boards)
    {
        AssertAssume(i.board.playerOnMove == onMoveResearched);//Check if all boards are from the same POV. Required for the a/B to work.

        if (i.variationDepth != 0) [[likely]]//If not draw by repetition
            {
                //previousDepth = std::min(previousDepth,i.variationDepth);
                i.variationDepth = depth;
            }

        i.time = static_cast<duration_t>(std::numeric_limits<double>::infinity()); //To know which failed to finish
        i.pruned = false;
    }


    auto timeThisStarted = std::chrono::high_resolution_clock::now();
    if (depth > 0)
    {
        alphaOrBeta = pricePiece(PieceGeneric::King) * onMoveResearched;
        //lastReportedLowerBound = alphaOrBeta;
        //transpositions.clear();

        criticalTimeDepleted = false;
        onMoveW = onMoveResearched;
        //depthW = depth;
        //q.clear();

        auto oldBestMove = boards[0].firstMoveNotation;

        bestMove = nullptr;
        q = &boards;
        qPos = 0;


        //solvedMoves = &resultBoards;



        //q.reserve(boards.size());
        //for (auto& board : boards)
        //    q.push_back(&board);

        //if(options.MultiPV<=1)
        //    evaluateGameMoveFromQ(qPos++, depthW);//It is usefull to run first pass on single core at full speed to set up alpha/Beta


        workToDo = true;
        workToDo.notify_all();


        workerFromQ(0);//Do work on this thread

        barrier->arrive_and_wait();//Wait for the workers to finish


        stack_vector<Variation<>, maxMoves> resultBoards;

        //Add the best result
        if (bestMove != nullptr)
        {
            resultBoards.push_back(*bestMove);
        }

        //Add the rest of the boards that finished computation without changing the order
        for (auto&& i : boards)
        {
            if (&i != bestMove && i.time != static_cast<duration_t>(std::numeric_limits<double>::infinity()))
                resultBoards.push_back(std::move(i));
            //else
              //  debugOut << "board " << i.firstMoveNotation << " has value of infinity" << std::endl;
        }

        if (resultBoards.size() == availableMoves)
        {
            fullDepth = depth + 1;
        }
        if (resultBoards.size() != boards.size())
        {
            debugOut << "Not enough time to search all moves. Only managed to fully go through " << resultBoards.size() << " out of " << boards.size() << std::endl;

            for (const auto& i : resultBoards)
            {
                if (oldBestMove == i.firstMoveNotation)
                {
                    debugOut << "Time ran out, but the engine at least managed to include the supposed best move in the results. Returning results." << std::endl;
                    goto bestMoveFound;
                }
            }
            debugOut << "Time ran out and the engine did not manage to include the suppossed best move. Cancelling the results." << std::endl;
            return duration_t(std::numeric_limits<double>::infinity());

        }
    bestMoveFound:

        //Determine if pruning caused some upperbound results (we cannot calculate with them the same way)
        bool noUpperboundResults = true;
        for (const auto& i : resultBoards)
        {
            if (i.pruned)
            {
                noUpperboundResults = false;
                break;
            }
        }

        if (noUpperboundResults)
            debugOut << "All board evaluations are exact, performing sorting/shuffling!" << std::endl;
        else
            debugOut << "Some board evalutions are upper bound only, sorting/shuffling not possible." << std::endl;


        //Shuffle best results if required and best result is not a draw
        if (noUpperboundResults && resultBoards.size() > 2)
        {
            if (shuffle && bestMove->bestFoundValue != 0.0f)
            {
                std::shuffle(resultBoards.begin(), resultBoards.end(), rng);
            }

            //Sort only if we know the real value of all boards (no first level pruning occured)
            switch (onMoveResearched)
            {
            case PlayerSide::WHITE: {
                std::stable_sort(resultBoards.begin(), resultBoards.end(), std::less<Variation<>>());
            } break;
            case PlayerSide::BLACK: {
                std::stable_sort(resultBoards.begin(), resultBoards.end(), std::greater<Variation<>>());
            } break;
            default:
                std::unreachable();
            }
        }

        for (const auto& i : resultBoards)
            totalNodesDepth += i.nodes;

        totalNodesAll += totalNodesDepth;

        for (size_t i = 0; i < resultBoards.size() && timeFirstBoard.count() == 0; ++i)
            timeFirstBoard = resultBoards[i].time;

        if (timeFirstBoard.count() == 0) [[unlikely]] //All are 0 - all moves lead to a draw by repetition. No need to search further
            timeFirstBoard = duration_t(std::numeric_limits<double>::infinity());

        boards = std::move(resultBoards);
    }
    duration_t elapsedThis = std::chrono::high_resolution_clock::now() - timeThisStarted;
    duration_t elapsedTotal = std::chrono::high_resolution_clock::now() - timeGlobalStarted;

    if (options.Verbosity >= 6 || options.MultiPV > 1)
    {
        for (size_t i = boards.size() - 1; i != 0; --i)
            printMoveInfo(depth + 1, elapsedTotal, elapsedThis, totalNodesDepth, boards[i], i + 1, oppositeSide(onMoveW));
    }

    if (options.Verbosity >= 1) //TODO: make it available to print later for the last depth if verbosity is set to 1, do not print every time unless >= 2.
    {
        printMoveInfo(depth + 1, elapsedTotal, elapsedThis, totalNodesDepth, boards.front(), 1, oppositeSide(onMoveW));
        std::cout << std::flush;
    }

    return timeFirstBoard;
}

//std::pair<GameState, float> findBestOnTopLevel(GameState& board, i8 depth, PlayerSide onMove)
//{
//    auto tmp = generateMoves(board, onMove);//, onMove);
//    findBestOnSameLevel(tmp, depth - 1);//, oppositeSide(onMove));
//    const auto& res = tmp.front();
//    return { res.board, res.bestFoundValue };
//}



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

void executeMove(GameState& board, std::string_view& str, stack_vector<std::array<Piece, 64>,75>& playedPositionsWhite, stack_vector<std::array<Piece, 64>, 75>& playedPositionsBlack)
{
    auto move = getWord(str);

    auto backup = board.pieceAt(move[2], move[3]);
    if (backup == Piece::Nothing && toGenericPiece(board.pieceAt(move[0], move[1])) != PieceGeneric::Pawn)
    {
        board.repeatableMoves += 1;

        if (board.playerOnMove == PlayerSide::WHITE)
            playedPositionsWhite.push_back(board.board);
        else
            playedPositionsBlack.push_back(board.board);
    }
    else
    {
        board.repeatableMoves = 0;
        playedPositionsWhite.clear();
        playedPositionsBlack.clear();
    }

    board.movePiece(move[0], move[1], move[2], move[3]);

    //Castling
    if      (move == "e1c1" && board.pieceAt(move[2], move[3]) == Piece::KingWhite)
        board.movePiece('a', '1', 'd', '1');
    else if (move == "e1g1" && board.pieceAt(move[2], move[3]) == Piece::KingWhite)
        board.movePiece('h', '1', 'f', '1');
    else if (move == "e8c8" && board.pieceAt(move[2], move[3]) == Piece::KingBlack)
        board.movePiece('a', '8', 'd', '8');
    else if (move == "e8g8" && board.pieceAt(move[2], move[3]) == Piece::KingBlack)
        board.movePiece('h', '8', 'f', '8');

    //Pawn promotion
    else if (move.size() == 5)
    {
        char promotionChar = tolower(move[4]);
        Piece evolvedInto = fromGenericPiece(fromGenericSymbol(promotionChar), board.playerOnMove);
        board.pieceAt(move[2], move[3]) = evolvedInto;
    }

    //En passant
    else if (move[1] == '5' && board.pieceAt(move[2], move[3]) == Piece::PawnWhite && move[0] != move[2] && backup == Piece::Nothing)
    {
        board.pieceAt(move[2], move[3] - 1) = Piece::Nothing;
    }
    else if (move[1] == '4' && board.pieceAt(move[2], move[3]) == Piece::PawnBlack && move[0] != move[2] && backup == Piece::Nothing)
    {
        board.pieceAt(move[2], move[3] + 1) = Piece::Nothing;
    }

    //Castling posibility invalidation
    //Doesn't matter when later in the game the piece no longer stands in original position -> it must have left before
    switch (move[0])
    {
    case('e'): {
        switch (move[1])
        {
        case('1'):
        {
            //White moves king
            board.canCastle[0][index(PlayerSide::WHITE)] = false;
            board.canCastle[1][index(PlayerSide::WHITE)] = false;
        } break;
        case('8'):
        {
            //Black moves king
            board.canCastle[0][index(PlayerSide::BLACK)] = false;
            board.canCastle[1][index(PlayerSide::BLACK)] = false;
        } break;
        default:
            break;
        }
    } break;
    case('a'): {
        switch (move[1])
        {
        case('1'): {
            board.canCastle[0][index(PlayerSide::WHITE)] = false;//White moves left rook
        } break;
        case('8'): {
            board.canCastle[0][index(PlayerSide::BLACK)] = false;//Black moves left rook
        } break;
        default:
            break;
        }
    } break;
    case('h'): {
        switch (move[1])
        {
        case ('1'): {
            board.canCastle[1][index(PlayerSide::WHITE)] = false;//White moves right rook
        } break;
        case ('8'): {
            board.canCastle[1][index(PlayerSide::BLACK)] = false;//Black moves right rook
        } break;
        default:
            break;
        }
    } break;

    default:
        break;
    }

    board.playerOnMove = oppositeSide(board.playerOnMove);
}

void parseMoves(GameState& board, std::string_view str, stack_vector<std::array<Piece, 64>, 75>& playedPositions)
{
    if (getWord(str) != "moves")
        return;

    stack_vector<std::array<Piece, 64>, 75> playedPositionsWhite;
    stack_vector<std::array<Piece, 64>, 75> playedPositionsBlack;

    while (!str.empty())
    {
        executeMove(board, str, playedPositionsWhite, playedPositionsBlack);
    }

    if (board.playerOnMove == PlayerSide::WHITE)
        playedPositions = std::move(playedPositionsBlack);
    else
        playedPositions = std::move(playedPositionsWhite);
}

GameState posFromFen(std::string_view& fen)
{
    GameState res;

    size_t i = 0;
    // Position
    {
        size_t pos = 64-8;
        while (fen[i] != ' ')
        {
            char c = fen[i++];

            if (c == '/')
            {
                assert(pos % 8 == 0); // Invalid fen string
                pos -= 16;
            }
            else if (isdigit(c))
            {
                pos += c - '0'; // This many fields are empty
            }
            else
            {
                assert(pos < 64);
                res.board[pos++] = fromSymbol(c);
            }
        }
        assert(pos == 8);
    }

    // Space
    assert(fen[i] == ' ');
    ++i;

    // Active color
    switch (fen[i++])
    {
    case 'w':
        res.playerOnMove = PlayerSide::WHITE;
        break;
    case 'b':
        res.playerOnMove = PlayerSide::BLACK;
        break;
    default:
        std::unreachable();
    }

    // Space
    assert(fen[i] == ' ');
    ++i;

    // Castling
    res.canCastle = { {{false,false},{false,false}} };
    if (fen[i] != '-')
    {
        while (fen[i] != ' ')
        {
            switch (fen[i++])
            {
            case 'K':
                res.canCastle[1][1] = true;
                break;
            case 'Q':
                res.canCastle[0][1] = true;
                break;
            case 'k':
                res.canCastle[1][0] = true;
                break;
            case 'q':
                res.canCastle[0][0] = true;
                break;
            default:
                if (options.UCI_Chess960)
                {
                    //TODO
                }
                else
                    std::unreachable();
                break;
            }
        }
    }
    else
        ++i;
    static_assert(index(PlayerSide::BLACK) == 0 && index(PlayerSide::WHITE) == 1); // We are assuming this in the array above

    // Space
    assert(fen[i] == ' ');
    ++i;

    // En passant
    if (fen[i] != '-')
    {
        char column = fen[i++];
        char row = fen[i++];

        // En passant not implemented, TODO
    }
    else
        ++i;

    // Space
    assert(fen[i] == ' ');
    ++i;

    // Halfmove clock

    res.repeatableMoves = 0;

    while (fen[i] != ' ')
    {
        res.repeatableMoves *= 10;
        res.repeatableMoves += (fen[i++] - '0');
    }

    // Space
    assert(fen[i] == ' ');
    ++i;

    // Fullmove clock

    i32 fullMoves = 0;

    while (i<fen.size() && fen[i++] != ' ')
    {
        fullMoves *= 10;
        fullMoves += (fen[i] - '0') * 10;
    }

    fen = fen.substr(i);
    return res;
}

GameState posFromString(std::string_view str, stack_vector<std::array<Piece, 64>, 75>& playedPositions)
{
    std::optional<GameState> res;
    auto word = getWord(str);
    if (word == "startpos") [[likely]]
    {
        res.emplace(GameState::startingPosition());
    }
    else if (word == "fen")
    {
        res.emplace(posFromFen(str));

        //assert(str[0] == ' ');
        //str = str.substr(1);
    }
    else
        std::unreachable();

    parseMoves(*res, str, playedPositions);

#ifdef _DEBUG
    res->print(debugOut);
#endif

    return *res;
}

Variation<> findBestInNumberOfMoves(GameState& board, i8 moves)
{
    timeGlobalStarted = std::chrono::high_resolution_clock::now();
    //dynamicPositionRanking = false;
    auto boardList = generateMoves(board, board.playerOnMove, {});

    //for (const auto& i : boardList)
    //{
    //    out << i.bestFoundValue << std::endl;
    //    i.researchedBoard.print();
    //    out << std::endl;
    //}
    //boardList.erase(boardList.begin());

    Variation res;
    //dynamicPositionRanking = true;

    out << "Depth: ";
    for (i8 i = 4; i <= moves; i += 2) {
        auto bestPosFound = findBestOnSameLevel(boardList, i);
        //dynamicPositionRanking = false;
        if (criticalTimeDepleted)
            break;
        else
        {
            res = boardList.front();
        }
        out << i << ' ';
    }
    out << std::endl;

    return res;
}

std::mutex uciGoM;
//bool ponder = false;//TODO finish pondering

duration_t predictMultiSearchTime(duration_t maxSearchTime, size_t nextMoveCount)
{
    float threadLoops = ceil(1.0f * nextMoveCount / options.Threads);
    return maxSearchTime * threadLoops;
    //return maxSearchTime;
}

duration_t predictTime(const duration_t& olderTime, const duration_t& newerTime, size_t movesCount)
{
    double logOlder = log2(olderTime.count());
    double logNewer = log2(newerTime.count());

    double growth = logNewer + logNewer - logOlder;

    constexpr double k = 1.0;//extra branching factor
    growth *= k;

    debugOut << "branching factor is " << growth << std::endl;
    auto projectedNextTime = predictMultiSearchTime(duration_t(pow(2, growth)), movesCount);
    debugOut << "Projected next time is " << projectedNextTime.count() << " ms." << std::endl;
    return projectedNextTime;
}

void uciGo(GameState& board, std::array<duration_t, 2> playerTime, std::array<duration_t, 2> playerInc, duration_t timeTarget, size_t maxDepth, const stack_vector<std::array<Piece, 64>, 75>& playedPositions)
{
    std::unique_lock l(uciGoM);
    timeGlobalStarted = std::chrono::high_resolution_clock::now();
    //uciGoM.lock();

    duration_t timeTargetMax(timeTarget);
    duration_t timeTargetOptimal(timeTarget);

    if (timeTargetMax == duration_t(0))//We have to calculate our own time
    {
        float gamePhase = (17.0f - board.countPiecesMin()) / 16.0f;

        const auto& myTime = playerTime[index(board.playerOnMove)];// == PlayerSide::WHITE ? wtime : btime;
        const auto& myInc = playerInc[index(board.playerOnMove)];// == PlayerSide::WHITE ? winc : binc;

        timeTargetMax = duration_t(((myTime * gamePhase) / 3));
        timeTargetOptimal = myInc + timeTargetMax / 3;

        //if (ponder)
        //{
        //    debugOut << "We are pondering, enemy time is our time" << std::endl;
        //    timeTargetMax += playerTime[(oppositeSide(board.playerOnMove) + 1) / 2];
        //    timeTargetOptimal += playerTime[(oppositeSide(board.playerOnMove) + 1) / 2];
        //}
    }


    debugOut << "Targeting " << timeTargetOptimal.count() << " ms." << std::endl;
    debugOut << "Highest I can go is " << timeTargetMax.count() << " ms." << std::endl;

    criticalTimeDepleted = false;
    optimalTimeDepleted = false;
    std::mutex mTimeoutCritical;
    std::mutex mTimeoutOptimal;
    std::condition_variable cvTimeout;
    std::atomic<uint8_t> timeoutThreadsWaiting = 0;
    std::thread criticalTimeout;
    std::thread optimalTimeout;
    std::atomic<bool> threadsEndWanted = false;


    auto boardList = generateMoves(board,board.playerOnMove, playedPositions);
    availableMoves = boardList.size();


    moveNotation bestPosFound;

    if (boardList.size() == 1) [[unlikely]]//If there is only one possible move to be played, no need to think about anything
    {
        debugOut << "Only one move possible, no need to think about anything" << std::endl;
        bestPosFound = std::move(boardList[0].firstMoveNotation);
    }
    else
    {
        stack_vector<std::pair<duration_t, moveNotation>, 256> previousResults;

        size_t i = 4;

        if (availableMoves > options.Threads) //Optimization for A/B pruning
            firstLevelPruning = false; //Depth 4 no pruning

        //Full depth search
        {
            //stack_vector<duration_t, 2> previousResultsFullTime;
            for (; i <= 6; i += 2)
            {
                //Loking through everything in a specific depth, no cutoffs, trying to find even unlikely good moves

                //fullDepth = i;
                if (options.Verbosity >= 2)
                    out << "info depth " << i << nl << std::flush;


                //size_t availableMoveCount = boardList.size();

                uciGoM.unlock();//Allow interruption in the middle of a calculation
                auto startThisLayer = std::chrono::high_resolution_clock::now();
                duration_t firstMoveElapsed = findBestOnSameLevel(boardList, i);
                auto elapsedThisLayer = duration_t(std::chrono::high_resolution_clock::now() - startThisLayer);
                uciGoM.lock();



                debugOut << "Elapsed for this layer: " << elapsedThisLayer.count() << std::endl;
                //previousResultsFullTime.emplace_back(elapsedThisLayer);

                bestPosFound = boardList.front().firstMoveNotation;


                previousResults.emplace_back(firstMoveElapsed, boardList.front().firstMoveNotation);
                debugOut << "Elapsed for the first move: " << firstMoveElapsed.count() << std::endl;

                if (round(abs(boardList.front().bestFoundValue) / matePrice) > 0)
                {
                    debugOut << "Mate possibility, no need to search further" << std::endl;
                    goto returnResult;
                }

                if (duration_t(std::chrono::high_resolution_clock::now() - timeGlobalStarted) >= timeTargetMax)//Emergency stop if we depleted time
                {
                    debugOut << "Time ran out while in the broad depth mode" << std::endl;
                    goto returnResult;
                }


                if (options.MultiPV == 1)
                    firstLevelPruning = true;
                else
                    firstLevelPruning = false;
            }
        }

        timeoutThreadsWaiting = 2;

        if (timeTargetMax != duration_t(std::numeric_limits<double>::infinity()))
        {
            criticalTimeout = std::thread([&]()
                {
                    std::unique_lock<std::mutex> l(mTimeoutCritical);
                    while (true)
                    {
                        auto res = cvTimeout.wait_for(l, timeTargetMax - duration_t(5));
                        switch (res)
                        {
                        case std::cv_status::no_timeout:[[likely]]
                            if (threadsEndWanted)[[likely]]
                                goto end;
                            break;//spurious wakeup
                        case std::cv_status::timeout:
                            criticalTimeDepleted = true;
                            debugOut << std::endl << std::endl << "Critical timeout! Terminating running computations ASAP" << std::endl << std::endl << std::endl;
                            goto end;
                            break;
                        default:
                            std::unreachable();
                        }
                    }
                    end:
                    --timeoutThreadsWaiting;
                });
            //threadTimeout.detach();
        }
        if (timeTargetOptimal != duration_t(std::numeric_limits<double>::infinity()))
        {
            optimalTimeout = std::thread([&]()
                {
                    std::unique_lock<std::mutex> l(mTimeoutOptimal);
                    while (true)
                    {
                        auto res = cvTimeout.wait_for(l, timeTargetOptimal);
                        switch (res)
                        {
                        case std::cv_status::no_timeout:[[likely]]
                            if (threadsEndWanted) [[likely]]
                                goto end;
                                break;//spurious wakeup
                        case std::cv_status::timeout:
                            optimalTimeDepleted = true;
                            debugOut << std::endl << std::endl << "Optimal timeout! Not allowing any more variations, but finishing what already started." << std::endl << std::endl << std::endl;
                            goto end;
                            break;
                        default:
                            std::unreachable();
                        }
                    }
                end:
                    --timeoutThreadsWaiting;
                });
            //threadTimeout.detach();
        }



        //Continuing full depth search while in optimal time window
        {
            for (; i <= maxDepth; i += 2)
            {
                auto projectedNextTime = predictTime(previousResults[previousResults.size() - 2].first, previousResults[previousResults.size() - 1].first, boardList.size());



                if (projectedNextTime > (timeTargetOptimal - duration_t(std::chrono::high_resolution_clock::now() - timeGlobalStarted)))//.
                {
                    debugOut << "It's time to end broad depth mode" << std::endl;
                    break;
                }
                else
                {
                    debugOut << "There is plenty of time, continuing the broad depth mode" << std::endl;
                }

                //Loking through everything in a specific depth, no cutoffs, trying to find even unlikely good moves

                //fullDepth = i;
                if (options.Verbosity >= 2)
                    out << "info depth " << i << nl << std::flush;


                //size_t availableMoveCount = boardList.size();

                uciGoM.unlock();//Allow interruption in the middle of a calculation
                //auto startThisLayer = std::chrono::high_resolution_clock::now();
                duration_t firstMoveElapsed = findBestOnSameLevel(boardList, i);
                //auto elapsedThisLayer = duration_t(std::chrono::high_resolution_clock::now() - startThisLayer);
                uciGoM.lock();

                debugOut << "Elapsed for the first move: " << firstMoveElapsed.count() << std::endl;


                //previousResults[0] = std::move(previousResults[1]);
                //previousResults.pop_back();
                previousResults.emplace_back(firstMoveElapsed, boardList.front().firstMoveNotation);

                //debugOut << "Elapsed for this layer: " << elapsedThisLayer.count() << std::endl;
                //previousResultsFullTime.emplace_back(elapsedThisLayer);


                bestPosFound = boardList.front().firstMoveNotation;

                if (round(abs(boardList.front().bestFoundValue) / matePrice) > 0)
                {
                    debugOut << "Mate possibility, no need to search further" << std::endl;
                    goto returnResult;
                }

                auto elapsedTotal = duration_t(std::chrono::high_resolution_clock::now() - timeGlobalStarted);
                if (elapsedTotal >= timeTargetMax)//Emergency stop if we depleted time
                {
                    debugOut << "Time ran out while in the broad depth mode" << std::endl;
                    goto returnResult;
                }
            }
        }

        //previousResults.pop_back();//Next prediction will be from selected moves

        //i += 2;

        debugOut << "Transitioning to selective depth mode" << std::endl;

        //i += 2;

        //Seldepth search
        {
            float centiPawnBreakingPoint = 512;
            for (; i <= maxDepth; i += 2)
            {
                bool cutOff = cutoffBadMoves(boardList, centiPawnBreakingPoint);


                if (boardList.size() == 1)
                {
                    debugOut << "Only one good move after cutoff, no need to search further" << std::endl;
                    goto returnResult;
                }



                //We have enough data to predict next move time

                auto projectedNextTime = predictTime(previousResults[previousResults.size() - 2].first, previousResults[previousResults.size() - 1].first, boardList.size());
                if (projectedNextTime > (timeTargetMax - duration_t(std::chrono::high_resolution_clock::now() - timeGlobalStarted)))
                {
                    debugOut << "We wouldn't get a result in required time" << std::endl;
                    goto returnResult;
                }
                else
                {
                    bool foundSameBestMove = previousResults[previousResults.size() - 1].second == previousResults[previousResults.size() - 2].second;
                    double diff = std::abs(previousResults[previousResults.size() - 1].first.count() - previousResults[previousResults.size() - 2].first.count());

                    if (foundSameBestMove || diff < 50)//The difference is too small, we probably wouldn't get much further info.
                    {
                        debugOut << "We could start another depth, but it's probably not necessary." << std::endl;
                        break;
                    }
                    else
                        debugOut << "This time, we are using some extra time to really think about this move." << std::endl;
                }


                if (options.Verbosity >= 2)
                {
                    i8 fullDepthInfo = fullDepth;
                    if (boardList.size() == availableMoves)
                        fullDepthInfo += 2;

                    out << "info depth " << (unsigned)fullDepthInfo;

                    if (i != fullDepthInfo)
                        out << " seldepth " << (unsigned)i;

                    out << nl << std::flush;
                }


                uciGoM.unlock();//Allow interruption in the middle of a calculation
                duration_t firstMoveElapsed = findBestOnSameLevel(boardList, i);
                uciGoM.lock();

                bestPosFound = boardList.front().firstMoveNotation;

                if (duration_t(std::chrono::high_resolution_clock::now() - timeGlobalStarted) >= timeTargetMax)//Emergency stop if we depleted time
                {
                    debugOut << "Emergency stop!" << std::endl;
                    goto returnResult;
                }

                if (round(abs(boardList.front().bestFoundValue) / matePrice) > 0)
                {
                    debugOut << "Mate possibility, no need to search further" << std::endl;
                    goto returnResult;
                }


                previousResults.emplace_back(firstMoveElapsed, boardList.front().firstMoveNotation);

                centiPawnBreakingPoint /= 2;
            }
        }
    }


    returnResult:
    out << "bestmove " << bestPosFound << nl << std::flush;

    if (timeTargetMax != duration_t(std::numeric_limits<double>::infinity()))
    {
        threadsEndWanted = true;
        do
        {
            cvTimeout.notify_all();//Cancel the timeout timer
        } while (timeoutThreadsWaiting != 0);//Have to wait for the threads to be notified, if not started yet

        if(criticalTimeout.joinable())
            criticalTimeout.join();

        if(optimalTimeout.joinable())
            optimalTimeout.join();
    }
}


int uci(std::istream& in, std::ostream& output)
{
    outStream = &output;
    //while (true)
    //{
        //std::string command;
        //std::cin >> command;
        //if (command == "isready")
        //{
        //    out << "readyok" << std::endl;
    GameState board;
    stack_vector<std::array<Piece, 64>, 75> playedPositions;
    //std::ofstream debugOut("debug.log");

    while (true)
    {
        std::string command;
        std::getline(in, command);
        
        if (!in.good()) [[unlikely]]
        {
            debugOut << "End of input stream, rude! End the uci session with 'quit' in a controlled way." << std::endl;
            threadRestart(0);
            return 0;
        }
            
        std::string_view commandView(command);
        auto commandFirst = getWord(commandView);

        if (commandFirst == "uci")
        {
            {
                //board = GameState();
#ifdef _DEBUG
                options.MultiPV = 1;// maxMoves;
                options.Threads = 1;
                options.Verbosity = 7;
#else
                options.MultiPV = 1;
                options.Threads = std::max(std::thread::hardware_concurrency() / 2, 1u);
                options.Verbosity = 3;
#endif
                options.UCI_Chess960 = false;
            }

            if (threadWorkers.size() != options.Threads)
                threadRestart(options.Threads);

            out << "id name Klara Destroyer" << nl
                << "id author Matej Kocourek" << nl
                << "option name MultiPV type spin min 1 max 218 default " << options.MultiPV << nl
                << "option name Threads type spin min 1 max 255 default " << options.Threads << nl
                << "option name Verbosity type spin min 0 max 7 default " << options.Verbosity << nl
                //<< "option name UCI_Chess960 type check default false" << nl
                << "uciok" << nl
                << std::flush;
        }
        else if (commandFirst == "ucinewgame")
        {
            board = GameState();
        }
        else if (commandFirst == "isready")
        {
            std::unique_lock l(uciGoM);
            out << "readyok" << nl << std::flush;
        }
        else if (commandFirst == "quit")
        {
            threadRestart(0);
            debugOut << "Bye!" << std::endl;
            return 0;
        }
        else if (commandFirst == "position")
        {
            std::unique_lock l(uciGoM);
            playedPositions.clear();
            board = posFromString(commandView, playedPositions);
        }
        else if (commandFirst == "setoption")
        {
            std::unique_lock l(uciGoM);
            if (getWord(commandView) != "name")
                continue;
            auto optionName = getWord(commandView);
            if (getWord(commandView) != "value")
                continue;
            auto optionValue = getWord(commandView);
            if (optionName == "MultiPV")
            {
                //debugOut << "Option value was: " << optionValue << std::endl;
                options.MultiPV = std::atoll(optionValue.data());
                debugOut << "Setting MultiPV to " << options.MultiPV << std::endl;
            }
            else if (optionName == "Threads")
            {
                //debugOut << "Option value was: " << optionValue << std::endl;
                options.Threads = std::atoll(optionValue.data());
                debugOut << "Setting Threads to " << options.Threads << std::endl;
                if (threadWorkers.size() != options.Threads)
                    threadRestart(options.Threads);
            }
            else if (optionName == "Verbosity")
            {
                //debugOut << "Option value was: " << optionValue << std::endl;
                options.Verbosity = std::atoll(optionValue.data());
                debugOut << "Setting Verbosity to " << options.Verbosity << std::endl;
            }
            else if (optionName == "UCI_Chess960")
            {
                options.UCI_Chess960 = (optionValue == "true");
                debugOut << "Setting UCI_Chess960 to " << options.UCI_Chess960 << std::endl;
            }
            else
            {
                debugOut << "This option is not recognized. Skipping." << std::endl;
            }
        }
        else if (commandFirst == "go")
        {
            std::array<duration_t, 2> playerTime = { duration_t(0), duration_t(0) };
            std::array<duration_t, 2> playerInc = { duration_t(0), duration_t(0) };
            //int64_t wtime = 0, btime = 0, winc = 0, binc = 0;
            duration_t timeTarget(0);
            i8 maxDepth = std::numeric_limits<i8>::max();

            while (true)
            {
                auto word = getWord(commandView);
                if (word.empty())
                    break;
                else if (word == "wtime")
                    playerTime[index(PlayerSide::WHITE)] = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                else if (word == "btime")
                    playerTime[index(PlayerSide::BLACK)] = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                else if (word == "winc")
                    playerInc[index(PlayerSide::WHITE)] = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                else if (word == "binc")
                    playerInc[index(PlayerSide::BLACK)] = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                else if (word == "movetime")
                {
                    timeTarget = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                }
                else if (word == "infinite")
                {
                    timeTarget = duration_t(std::numeric_limits<double>::infinity());
                }
                else if (word == "depth")
                    maxDepth = atoll(getWord(commandView).data());
            }
            
            uciGo(board, playerTime, playerInc, timeTarget, maxDepth, playedPositions);
        }
        else
        {
            debugOut << "Not recognized command, ignoring: " << commandFirst << std::endl;
        }
    }
    //}
//}
}


void benchmark(i8 depth, GameState board)
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
    out << result.firstMoveNotation << std::endl;

    out << "cp: " << result.bestFoundValue << std::endl;//<<"Total found score "<<result.second+result.first.balance()<<endl;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
    out << "Done in " << elapsed << "s." << std::endl;

    //board.printW();
}

}
