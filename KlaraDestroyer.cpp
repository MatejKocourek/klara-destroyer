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
#include <functional>
#include <sstream>
#include <iomanip>

#define out std::cout
#define nl '\n'

#ifdef _DEBUG
#define AssertAssume(...)  assert(__VA_ARGS__)
#else
#define AssertAssume(...)  __assume(__VA_ARGS__)
#endif



//#define CASTLING_DISABLED

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

static constexpr float kingPrice = 20000;
static constexpr float matePrice = 10000000;
static constexpr float stalePrice = 50000;

#define MOVE_PIECE_FREE_ONLY std::equal_to<float>()
#define MOVE_PIECE_CAPTURE_ONLY std::greater<float>()
#define MOVE_PIECE_FREE_CAPTURE std::greater_equal<float>()

enum PlayerSide : i8
{
    WHITE = 1,
    BLACK = -1,
    NONE = 0
};

enum PieceGeneric : i8
{
    NothingGeneric = 0,
    Pawn = 1,//0x01,
    Knight = 2,//0x02,
    Bishop = 3,//0x03,
    Rook = 4,//0x04,
    Queen = 5,//0x05,
    King = 6,//0x06,
};

enum Piece : i8
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

static bool itsTimeToStop;//atomic too slow
static constexpr bool dynamicPositionRanking = false;//Not needed, static ranking is faster and should be enough
static i8 depthW;
static PlayerSide onMoveW;
static bool deterministic;
static std::chrono::steady_clock::time_point timeGlobalStarted;
//static std::chrono::steady_clock::time_point timeDepthStarted;
static std::atomic<float> alphaOrBeta;
static constexpr i8 depthToStopOrderingPieces = 3;
static constexpr i8 depthToStopOrderingMoves = 3;

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


struct Options {
    size_t MultiPV = 1;
} options;

constexpr inline PlayerSide oppositeSide(PlayerSide side) noexcept
{
    AssertAssume(side==1||side==-1);
    return (PlayerSide)(-(i8)side);
}
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
    case Nothing:
        return L' ';
    case PawnWhite:
        return L'♙';
    case KnightWhite:
        return L'♘';
    case BishopWhite:
        return  L'♗';
    case RookWhite:
        return L'♖';
    case QueenWhite:
        return L'♕';
    case KingWhite:
        return L'♔';
    case PawnBlack:
        return L'♟';
    case KnightBlack:
        return L'♞';
    case BishopBlack:
        return L'♝';
    case RookBlack:
        return L'♜';
    case QueenBlack:
        return L'♛';
    case KingBlack:
        return L'♚';
    default:
        std::unreachable();
    }
    std::unreachable();
}

constexpr char symbolA(Piece p)
{
    switch (p)
    {
    case Nothing:
        return ' ';
    case PawnWhite:
        return 'p';
    case KnightWhite:
        return 'n';
    case BishopWhite:
        return 'b';
    case RookWhite:
        return 'r';
    case QueenWhite:
        return 'q';
    case KingWhite:
        return 'k';
    case PawnBlack:
        return 'P';
    case KnightBlack:
        return 'N';
    case BishopBlack:
        return 'B';
    case RookBlack:
        return 'R';
    case QueenBlack:
        return 'Q';
    case KingBlack:
        return 'K';
    default:
        std::unreachable();
    }
    std::unreachable();
}
constexpr PlayerSide pieceColor(Piece p)
{
    return static_cast<PlayerSide>(((static_cast<int8_t>(p) & static_cast <int8_t>(0x80)) >> 6) | 0x01);
}
constexpr PieceGeneric toGenericPiece(Piece p)
{
    return (PieceGeneric)((p >= 0) ? p : -p);
    //return static_cast<PieceGeneric>(std::abs(p));
}
constexpr float priceAdjustment(PieceGeneric p, i8 pieceIndex)
{
    //AssertAssume(column < 8 && row < 8 && column >= 0 && row >= 0);
    switch (p)
    {
    case Nothing:
        return 0;
    case Pawn:
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
    case Knight:
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
    case Bishop:
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
    case Rook:
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
    case Queen:
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
    case King:
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
    std::unreachable();
}
//constexpr float priceAdjustment(Piece p, i8 column, i8 row)
//{
//    return priceAdjustment(toGenericPiece(p), column, row);
//}
constexpr float priceAdjustmentPov(Piece p, i8 column, i8 row)
{
    if (p < 0)//case(PlayerSide::BLACK):
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
    case Nothing:
        return 0;
    case Pawn:
        return 82;
    case Knight:
        return 337;
    case Bishop:
        return 365;
    case Rook:
        return 477;
    case Queen:
        return 1025;
    case King:
        return kingPrice;
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
    case WHITE:
        return 7;
    case BLACK:
        return 0;
    default:
        std::unreachable();
    }
}

constexpr i8 initialRow(Piece p) {
    switch (p)
    {
    case PawnWhite:
        return 1;
    case KnightWhite:
    case BishopWhite:
    case RookWhite:
    case QueenWhite:
    case KingWhite:
        return 0;
    case PawnBlack:
        return 6;
    case KnightBlack:
    case BishopBlack:
    case RookBlack:
    case QueenBlack:
    case KingBlack:
        return 7;
    default:
        std::unreachable();
    }
}

std::array<Piece, 4> availablePromotes(Piece p)
{
    switch (p)
    {
    case PawnWhite:
    {
        static std::array<Piece, 4> whiteEvolveLastRow{ Piece::QueenWhite, Piece::RookWhite, Piece::BishopWhite, Piece::KnightWhite };
        return whiteEvolveLastRow;
    }
    case PawnBlack:
    {
        static std::array<Piece, 4> blackEvolveLastRow{ Piece::QueenBlack, Piece::RookBlack, Piece::BishopBlack, Piece::KnightBlack };
        return blackEvolveLastRow;
    }
    default:
        std::unreachable();
    }

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

struct BoardHasher
{
    std::size_t operator()(const Board& s) const noexcept;
};





class Board {
    //Piece* pieces[64];
public:
    std::array<Piece, 64> pieces;
    PlayerSide playerOnMove;
    std::array<bool, 2> canCastleLeft;
    std::array<bool, 2> canCastleRight;

    auto operator<=>(const Board&) const noexcept = default;

    Board(const Board& copy) = default;

    Board() : pieces{ Piece::Nothing }
    {
        //for (auto& i : pieces) {
        //    i = nullptr;
        //}
    }

    constexpr Board(std::array<Piece, 64> pieces, PlayerSide playerOnMove, std::array<bool, 2> canCastleLeft, std::array<bool, 2> canCastleRight):pieces(pieces),playerOnMove(playerOnMove),canCastleLeft(canCastleLeft),canCastleRight(canCastleRight)
    {}

    Board& operator=(const Board& copy) = default;

    constexpr std::array<i8, 128> piecesCount() const
    {
        std::array<i8, 128> res { 0 };
        for (const auto& i : pieces)
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
        return pieces[column + (row * 8)];
    }
    constexpr Piece& pieceAt(i8 column, i8 row)
    {
        AssertAssume(!(column < 0 || column > 7 || row < 0 || row > 7));
        return pieces[column + (row * 8)];
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
            throw std::exception("Trying to move an empty field");

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
        case BLACK:
        {
            for (i8 i = 0; i < 8; ++i) {
                os << i + 1 << ' ';
                for (i8 j = 7; j >= 0; --j) {
                    os << '|';

                    printPiece(pieces[j + i * 8], os);
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
                    printPiece(pieces[j + i * 8], os);
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


    std::array<char, 6> findDiff(const Board& old) const
    {
        std::array<char, 6> res = { 0 };

        Piece oldPiece = Piece::Nothing;

        for (size_t i = 0; i < pieces.size(); ++i)
        {
            if (pieces[i] == Piece::Nothing && old.pieces[i] != Piece::Nothing)
            {
                oldPiece = old.pieces[i];

                res[0] = i % 8 + 'a';
                res[1] = i / 8 + '1';

                if(toGenericPiece(old.pieces[i]) == PieceGeneric::King)//castling hack
                    break;
            }
        }
        if (oldPiece == Piece::Nothing)
            throw std::exception("No change made");
        for (size_t i = 0; i < pieces.size(); ++i)
        {
            if (pieces[i] != Piece::Nothing && pieces[i] != old.pieces[i])
            {
                res[2] = i % 8 + 'a';
                res[3] = i / 8 + '1';

                if (toGenericPiece(oldPiece) == PieceGeneric::Pawn && (res[3] == '1' || res[3] == '8'))
                {
                    res[4] = tolower(symbolA(pieces[i]));
                }

                if (pricePiece(pieces[i]) == kingPrice)//castling hack
                    break;
            }
        }
        return res;
    }
    
    constexpr float balance() const {
        double res = 0;
        for (i8 i = 0; i < 64; ++i) {
            if (pieces[i] == Piece::Nothing) [[likely]]//Just optimization
                continue;
            if (toGenericPiece(pieces[i]) == PieceGeneric::King) [[unlikely]]//To avoid floating point overflow
                continue;
            res += priceRelative(pieces[i], i % 8, i / 8);
        }
        return res;
    }

    
    constexpr i8 countPiecesMin() const
    {
        std::array<i8, 2> counters{ 0 };
        for (const auto& i : pieces)
        {
            if (i != Piece::Nothing)
                ++counters[(pieceColor(i) + 1)/2];
        }
        return std::min(counters[0], counters[1]);
    }

    constexpr static Board startingPosition()
    {
        return Board(
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
            PlayerSide::WHITE,
            { true, true },
            { true, true }
        );
        //initial.playerOnMove = PlayerSide::WHITE;
        //initial.canCastleLeft.fill(true);
        //initial.canCastleRight.fill(true);

        //initial.pieceAt('a', '1') = Piece::RookWhite;
        //initial.pieceAt('b', '1') = Piece::KnightWhite;
        //initial.pieceAt('c', '1') = Piece::BishopWhite;
        //initial.pieceAt('d', '1') = Piece::QueenWhite;
        //initial.pieceAt('e', '1') = Piece::KingWhite;
        //initial.pieceAt('f', '1') = Piece::BishopWhite;
        //initial.pieceAt('g', '1') = Piece::KnightWhite;
        //initial.pieceAt('h', '1') = Piece::RookWhite;

        //initial.pieceAt('a', '2') = Piece::PawnWhite;
        //initial.pieceAt('b', '2') = Piece::PawnWhite;
        //initial.pieceAt('c', '2') = Piece::PawnWhite;
        //initial.pieceAt('d', '2') = Piece::PawnWhite;
        //initial.pieceAt('e', '2') = Piece::PawnWhite;
        //initial.pieceAt('f', '2') = Piece::PawnWhite;
        //initial.pieceAt('g', '2') = Piece::PawnWhite;
        //initial.pieceAt('h', '2') = Piece::PawnWhite;

        //initial.pieceAt('a', '8') = Piece::RookBlack;
        //initial.pieceAt('b', '8') = Piece::KnightBlack;
        //initial.pieceAt('c', '8') = Piece::BishopBlack;
        //initial.pieceAt('d', '8') = Piece::QueenBlack;
        //initial.pieceAt('e', '8') = Piece::KingBlack;
        //initial.pieceAt('f', '8') = Piece::BishopBlack;
        //initial.pieceAt('g', '8') = Piece::KnightBlack;
        //initial.pieceAt('h', '8') = Piece::RookBlack;

        //initial.pieceAt('a', '7') = Piece::PawnBlack;
        //initial.pieceAt('b', '7') = Piece::PawnBlack;
        //initial.pieceAt('c', '7') = Piece::PawnBlack;
        //initial.pieceAt('d', '7') = Piece::PawnBlack;
        //initial.pieceAt('e', '7') = Piece::PawnBlack;
        //initial.pieceAt('f', '7') = Piece::PawnBlack;
        //initial.pieceAt('g', '7') = Piece::PawnBlack;
        //initial.pieceAt('h', '7') = Piece::PawnBlack;
        //return initial;
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





struct Variation {
    Board board;
    float bestFoundValue;
    float startingValue;
    //Board movePosition;
    i8 variationDepth;
    //std::chrono::duration<float, std::milli> researchedTime{0};

    PlayerSide firstMoveOnMove;
    std::array<char, 6> firstMoveNotation{ '\0' };

    std::vector<std::pair<Board, float>> firstPositions;
    bool saveToVector = false;
    //std::unordered_map<Board, float, BoardHasher> transpositions;

    size_t nodes = 0;


    Variation() = default;
    Variation(const Variation& copy) noexcept = default;//:researchedBoard(copy.researchedBoard),bestFoundValue(copy.bestFoundValue),pieceTakenValue(copy.pieceTakenValue){}
    Variation(Variation&& toMove) noexcept = default;// :researchedBoard(move(toMove.researchedBoard)), bestFoundValue(toMove.bestFoundValue), pieceTakenValue(toMove.pieceTakenValue) {}


    //Variation& operator=(Variation&& toMove) = default;

    Variation& operator=(const Variation& copy) = default;

    friend bool operator<(const Variation& l, const Variation& r)
    {
        return l.bestFoundValue < r.bestFoundValue;
    }
    friend bool operator>(const Variation& l, const Variation& r)
    {
        return l.bestFoundValue > r.bestFoundValue;
    }

    //Variation(Board researchedBoard, double bestFoundValue, double startingValue):researchedBoard(move(researchedBoard)),bestFoundValue(bestFoundValue), startingValue(startingValue) {}
    //Variation(Board board, float startingValue) :board(std::move(board)), bestFoundValue(startingValue), startingValue(startingValue) {}
    Variation(Board board, float bestFoundValue, float startingValue, i8 variationDepth, PlayerSide firstMoveOnMove, std::array<char, 6> firstMoveNotation) :board(std::move(board)), bestFoundValue(bestFoundValue), startingValue(startingValue), variationDepth(variationDepth), firstMoveOnMove(firstMoveOnMove), firstMoveNotation(firstMoveNotation){}




    bool canTakeKing(PlayerSide onMove)
    {
        float alpha, beta;
        alpha = -std::numeric_limits<float>::max();
        beta = std::numeric_limits<float>::max();

        TempSwap playerBackup(board.playerOnMove, onMove);
        TempSwap vectorBackup(saveToVector, false);

        TempSwap backupCastlingLeft(board.canCastleLeft, { false,false });
        TempSwap backupCastlingRight(board.canCastleRight, { false,false });

        for (i8 i = 0; i < board.pieces.size(); ++i) {
            Piece found = board.pieces[i];

            if (found == Piece::Nothing)
                continue;
            if (pieceColor(found) == onMove)
            {
                auto foundVal = bestMoveWithThisPieceScore((i % 8), (i / 8), 1, alpha, beta, 0);

                if (foundVal * onMove == kingPrice) [[unlikely]]//Je možné vzít krále, hra skončila
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
    //    for (i8 i = 0; i < pieces.size(); ++i) {
    //        const Piece* found = pieces[i];
    //        if (found == nullptr)
    //            continue;
    //        if (found->pieceColor() == attacker)
    //        {
    //            auto foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, valueSoFar);
    //            std::cerr << foundVal << std::endl;
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
        alpha = -std::numeric_limits<float>::max();
        beta = std::numeric_limits<float>::max();

        for (i8 i = 0; i < board.pieces.size(); ++i) {
            Piece found = board.pieces[i];

            if (found == Piece::Nothing)
                continue;
            if (pieceColor(found) == onMove)
            {
                auto foundVal = bestMoveWithThisPieceScore((i % 8), (i / 8), 1, alpha, beta, 0);

                if (foundVal != std::numeric_limits<float>::infinity() * (-1) * onMove)
                    return true;
            }
        }
        return false;
    }

    bool isValidSetup()
    {
        TempSwap saveVectorBackup(saveToVector, false);
        //bool backup = saveToVector;
        //saveToVector = false;
        //Both kings must be on the researchedBoard exactly once
        auto count = board.piecesCount();
        if (count['k'] != 1 || count['K'] != 1)
            return false;

        //print(std::cerr);

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
    //        if (pieces[i] != nullptr)
    //        {
    //            float alpha, beta;
    //            alpha = -std::numeric_limits<float>::max();
    //            beta = std::numeric_limits<float>::max();
    //            i32 totalMoves = 0;
    //            double totalValues = 0;
    //            pieces[i]->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, totalMoves, totalValues, 0,true)
    //            res += (((double)totalMoves)/2.0) * pieces[i]->pieceColor();
    //            if(totalMoves>0)
    //                res += std::min(std::max(totalValues,-100.0),100.0);
    //        }
    //    }
    //    return res / 10000.0;
    //}


    //template <bool saveToVector>
    float bestMoveScore(i8 depth, float valueSoFar, float alpha, float beta)
    {
        AssertAssume(board.playerOnMove == 1 || board.playerOnMove == -1);

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


        float bestValue = std::numeric_limits<float>::infinity() * (-1) * board.playerOnMove;

        //if (itsTimeToStop) [[unlikely]]
        //    return bestValue;

        if (depth > depthToStopOrderingPieces) [[unlikely]]
            {
                static_vector<std::pair<float, i8>, 16> possiblePiecesToMove;

                for (i8 i = 0; i < 64; ++i) {
                    float alphaTmp = alpha;
                    float betaTmp = beta;
                    const Piece found = board.pieces[i];

                    if (found == Piece::Nothing)
                        continue;
                    if (pieceColor(found) == board.playerOnMove)
                        possiblePiecesToMove.emplace_back(bestMoveWithThisPieceScore(i % 8, i / 8, 0, alphaTmp, betaTmp, valueSoFar), i);
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
                    const Piece found = board.pieces[i];
                    float foundVal;
                    if (depth > depthToStopOrderingMoves) [[unlikely]]
                        {
                            foundVal = bestMoveWithThisPieceScoreOrdered((i % 8), (i / 8), depth, alpha, beta, valueSoFar);
                        }
                    else
                    {
                        foundVal = bestMoveWithThisPieceScore((i % 8), (i / 8), depth, alpha, beta, valueSoFar);
                    }

                    if (depth == depthW && options.MultiPV == 1) [[unlikely]]
                        {
                            //std::osyncstream(std::cerr) << "alpha: " << alpha << ", beta: " << beta << std::endl;
                            switch (board.playerOnMove)
                            {
                            case PlayerSide::WHITE: {
                                beta = alphaOrBeta;
                            } break;
                            case PlayerSide::BLACK: {
                                alpha = alphaOrBeta;
                            } break;
                            default:
                                std::unreachable();
                            }
                            //std::osyncstream(std::cerr) << "alpha: " << alpha << ", beta: " << beta << std::endl;
                        }


                            if (foundVal * board.playerOnMove > bestValue * board.playerOnMove) {
                                bestValue = foundVal;
                            }
                        if (foundVal * board.playerOnMove == kingPrice)//Je možné vzít krále, hra skončila
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
        else [[likely]]
            {
                for (i8 i = 0; i < 64; ++i) {
                    const Piece found = board.pieces[i];

                    if (found == Piece::Nothing)
                        continue;
                    if (pieceColor(found) == board.playerOnMove)
                    {
                        auto foundVal = bestMoveWithThisPieceScore((i % 8), (i / 8), depth, alpha, beta, valueSoFar);

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
                        if (beta <= alpha)
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
                        TempSwap saveToVectorBackup(saveToVector, false);
                        if (canTakeKing(oppositeSide(board.playerOnMove)))//if (round((bestMoveScore(1, onMove * (-1),valueSoFar, alpha, beta) * playerOnMove * (-1))/100) == kingPrice/100)//Soupeř je v situaci, kdy mi může vzít krále (ohrožuje ho)
                        {
                            bestValue = -matePrice * board.playerOnMove;//Dostanu mat, co nejnižší skóre
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

    void placePieceAt(Piece p, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, bool& doNotContinue, float valueSoFar, float priceTaken)
    {
        ++nodes;
        if (saveToVector && depth == 0) [[unlikely]]
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

        //if (doNotContinue) [[unlikely]]
        //    return;

        if (priceTaken >= kingPrice - 128) [[unlikely]]//Je možné vzít krále
        {
            doNotContinue = true;
            bestValue = kingPrice * board.playerOnMove;
            //totalMoves++;
            //return;
        }
        else [[likely]]
        {
            float valueGained = priceTaken + priceAdjustmentPov(p, column, row); //We are entering new position with this piece

            const auto wholeMovesFromRoot = ((depthW - depth) / 2);//Kolikaty tah od initial pozice (od 0), ne pultah
            valueGained *= (1 - (wholeMovesFromRoot * 0.0001));//10000.0);

            valueSoFar += valueGained * board.playerOnMove;//Add our gained value to the score

            float foundVal;
            if (depth > 0)
            {
                foundVal = tryPiece(column, row, p, depth, alpha, beta, valueSoFar);

                if ((foundVal * board.playerOnMove * (-1)) == kingPrice)//V dalším tahu bych přišel o krále, není to legitimní tah
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

        doNotContinue |= (beta <= alpha);
    }


    template <typename F>
    bool tryPlacingPieceAt(Piece p, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, bool& doNotContinue, float valueSoFar, F condition)
    {
        if (doNotContinue)
            return false;

        float price = board.priceInLocation(column, row, board.playerOnMove);

        if (condition(price, 0))
        {
            placePieceAt(p, column, row, depth, alpha, beta, bestValue, doNotContinue, valueSoFar, price);
            return price == 0;
        }
        else
            return false;
    }

    auto tryPlacingPieceAt(Piece p, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, bool& doNotContinue, float valueSoFar)
    {
        return tryPlacingPieceAt(p, column, row, depth, alpha, beta, bestValue, doNotContinue, valueSoFar, MOVE_PIECE_FREE_CAPTURE);
    }

    template <typename T>
    bool addMoveToList(Piece p, i8 column, i8 row, float alpha, float beta, T& possibleMoves)
    {
        bool doNotContinueTmp = false;
        float bestValue = -std::numeric_limits<float>::infinity() * board.playerOnMove;
        bool fieldWasFree = tryPlacingPieceAt(p, column, row, 0, alpha, beta, bestValue, doNotContinueTmp, 0);

        if (bestValue != -std::numeric_limits<float>::infinity() * board.playerOnMove)
            possibleMoves.emplace_back(bestValue, std::make_pair(column, row));

        return fieldWasFree;
    }

    template <i8 rookColumn, i8 newRookColumn>
    void tryCastling(Piece p, i8 row, /*i8 kingColumn, i8 rookColumn, i8 newRookColumn,*/ bool& canICastleLeft, bool& canICastleRight, float& bestValue, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue)
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

        if (saveToVector) [[unlikely]]//To optimize time, we do not check whether the path is attacked. May produce bad predictions, but only for small number of cases.
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
            tryPlacingPieceAt(p, newKingColumn, row, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
            //State will be restored when calling destructors
    }

    float bestMoveWithThisPieceScore(i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNotContinue = false)
    {
        float bestValue = -std::numeric_limits<float>::infinity() * board.playerOnMove;

        Piece p = board.pieceAt(column, row);
        PieceGeneric piece = toGenericPiece(p);
        PlayerSide color = pieceColor(p);

        assert(color == board.playerOnMove);
        if (color == board.playerOnMove)
        {
            board.pieceAt(column, row) = Piece::Nothing;
            valueSoFar -= priceAdjustmentPov(p, column, row) * board.playerOnMove;//We are leaving our current position
            //board.playerOnMove = oppositeSide(board.playerOnMove);

            switch (piece)
            {
            case Nothing:
                break;
            case Pawn:
            {
                if (row + board.playerOnMove == promoteRow(board.playerOnMove)) [[unlikely]]
                    {
                        const auto& availableOptions = availablePromotes(p);//evolveIntoReference(row + advanceRow());
                        for (const auto& evolveOption : availableOptions) {
                            float valueDifferenceNextMove = (pricePiece(evolveOption) - pricePiece(piece)) * board.playerOnMove;//Increase in material when the pawn promotes
                            float valueSoFarEvolved = valueSoFar + valueDifferenceNextMove;

                            //Capture diagonally
                            tryPlacingPieceAt(evolveOption, column - 1, row + board.playerOnMove, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFarEvolved, MOVE_PIECE_CAPTURE_ONLY);
                            tryPlacingPieceAt(evolveOption, column + 1, row + board.playerOnMove, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFarEvolved, MOVE_PIECE_CAPTURE_ONLY);

                            //Go forward
                            tryPlacingPieceAt(evolveOption, column, row + board.playerOnMove, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFarEvolved, MOVE_PIECE_FREE_ONLY);
                        }
                    }
                else [[likely]]
                    {
                        //Capture diagonally
                        tryPlacingPieceAt(p, column - 1, row + board.playerOnMove, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar, MOVE_PIECE_CAPTURE_ONLY);
                        tryPlacingPieceAt(p, column + 1, row + board.playerOnMove, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar, MOVE_PIECE_CAPTURE_ONLY);

                        //Go forward
                        bool goForwardSuccessful = tryPlacingPieceAt(p, column, row + board.playerOnMove, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar, MOVE_PIECE_FREE_ONLY);
                        if (goForwardSuccessful) [[likely]]//Field in front of the pawn is empty, can make a second step
                            {
                                if (row == initialRow(p))//Two fields forward
                                {
                                    tryPlacingPieceAt(p, column, row + board.playerOnMove * 2, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar, MOVE_PIECE_FREE_ONLY);
                                }
                            }
                    }

            } break;
            case Knight:
            {
                tryPlacingPieceAt(p, column + 1, row + 2, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                tryPlacingPieceAt(p, column + 1, row - 2, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                tryPlacingPieceAt(p, column + 2, row + 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                tryPlacingPieceAt(p, column + 2, row - 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                tryPlacingPieceAt(p, column - 1, row + 2, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                tryPlacingPieceAt(p, column - 1, row - 2, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                tryPlacingPieceAt(p, column - 2, row + 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                tryPlacingPieceAt(p, column - 2, row - 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);

            } break;
            case Bishop:
            {
                for (i8 i = 1; tryPlacingPieceAt(p, column + i, row + i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column + i, row - i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column - i, row + i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column - i, row - i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
            } break;
            case Rook:
            {
                bool castleLeftBackup;
                bool castleRightBackup;

                if (initialRow(p) == row)
                {
                    if (column == 0)
                    {
                        bool& canICastleLeft = board.canCastleLeft[(board.playerOnMove + 1) / 2];
                        castleLeftBackup = canICastleLeft;
                        canICastleLeft = false;
                    }
                    else if (column == 7)
                    {
                        bool& canICastleRight = board.canCastleRight[(board.playerOnMove + 1) / 2];
                        castleRightBackup = canICastleRight;
                        canICastleRight = false;
                    }
                }

                for (i8 i = 1; tryPlacingPieceAt(p, column, row + i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column, row - i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column + i, row, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column - i, row, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);

                if (initialRow(p) == row)
                {
                    if (column == 0)
                    {
                        bool& canICastleLeft = board.canCastleLeft[(board.playerOnMove + 1) / 2];
                        canICastleLeft = castleLeftBackup;
                    }
                    else if (column == 7)
                    {
                        bool& canICastleRight = board.canCastleRight[(board.playerOnMove + 1) / 2];
                        canICastleRight = castleRightBackup;
                    }
                }
            } break;
            case Queen:
            {
                for (i8 i = 1; tryPlacingPieceAt(p, column + i, row + i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column + i, row - i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column - i, row + i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column - i, row - i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);

                for (i8 i = 1; tryPlacingPieceAt(p, column, row + i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column, row - i, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column + i, row, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);
                for (i8 i = 1; tryPlacingPieceAt(p, column - i, row, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar); ++i);

            } break;
            case King:
            {
                bool& canICastleLeft = board.canCastleLeft[(board.playerOnMove + 1) / 2];
                bool& canICastleRight = board.canCastleLeft[(board.playerOnMove + 1) / 2];


#ifndef CASTLING_DISABLED
                if (canICastleLeft)//Neither has moved
                {
                    AssertAssume(column == 4);//King has to be in initial position
                    AssertAssume(row == 0 || row == 7);
                    tryCastling<0, 3>(p, row, canICastleLeft, canICastleRight, bestValue, depth, alpha, beta, valueSoFar, doNotContinue);
                }
                if (canICastleRight)//Neither has moved
                {
                    AssertAssume(column == 4);//King has to be in initial position
                    AssertAssume(row == 0 || row == 7);
                    tryCastling<7, 5>(p, row, canICastleLeft, canICastleRight, bestValue, depth, alpha, beta, valueSoFar, doNotContinue);
                }
#endif

                //Classic king movement
                {
                    TempSwap castleLeftBackup(canICastleLeft, false);
                    TempSwap castleRightBackup(canICastleLeft, false);

                    tryPlacingPieceAt(p, column + 1, row + 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                    tryPlacingPieceAt(p, column + 1, row, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                    tryPlacingPieceAt(p, column + 1, row - 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                    tryPlacingPieceAt(p, column - 1, row + 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                    tryPlacingPieceAt(p, column - 1, row, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                    tryPlacingPieceAt(p, column - 1, row - 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                    tryPlacingPieceAt(p, column, row + 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                    tryPlacingPieceAt(p, column, row - 1, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);
                }
            } break;
            default:
                std::unreachable();
            }

            board.pieceAt(column, row) = p;
        }

        return bestValue;
    }

    //float bestMoveWithThisPieceScore(Piece p, Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNotContinue = false)
    //{
    //    return bestMoveWithThisPieceScore(toGenericPiece(p), board, column, row, depth, alpha, beta, valueSoFar, doNotContinue);
    //}

    float bestMoveWithThisPieceScoreOrdered(i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNotContinue = false)
    {
        //return bestMoveWithThisPieceScore(board, column, row, depth, alpha, beta, valueSoFar, doNotContinue);
        Piece p = board.pieceAt(column, row);
        PieceGeneric piece = toGenericPiece(p);
        PlayerSide color = pieceColor(p);

        switch (piece)
        {
        case Pawn:
        case Rook:
        case King:
            return bestMoveWithThisPieceScore(column, row, depth, alpha, beta, valueSoFar, doNotContinue);
        }
        float bestValue = -std::numeric_limits<float>::infinity() * board.playerOnMove;
        board.pieceAt(column, row) = Piece::Nothing;
        valueSoFar -= priceAdjustmentPov(p, column, row) * board.playerOnMove;//We are leaving our current position
        //board.playerOnMove = oppositeSide(board.playerOnMove);

        static_vector<std::pair<float, std::pair<i8, i8>>, 27> possibleMoves;

        switch (piece)
        {
        case Nothing:
            break;
        case Pawn:
        {
            //TODO
        } break;
        case Knight:
        {
            //static_vector<std::pair<float, std::pair<i8, i8>>, 8> possibleMoves;

            addMoveToList(p, column + 1, row + 2, alpha, beta, possibleMoves);
            addMoveToList(p, column + 1, row - 2, alpha, beta, possibleMoves);
            addMoveToList(p, column + 2, row + 1, alpha, beta, possibleMoves);
            addMoveToList(p, column + 2, row - 1, alpha, beta, possibleMoves);
            addMoveToList(p, column - 1, row + 2, alpha, beta, possibleMoves);
            addMoveToList(p, column - 1, row - 2, alpha, beta, possibleMoves);
            addMoveToList(p, column - 2, row + 1, alpha, beta, possibleMoves);
            addMoveToList(p, column - 2, row - 1, alpha, beta, possibleMoves);

        } break;
        case Bishop:
        {
            //static_vector<std::pair<float, std::pair<i8, i8>>, 13> possibleMoves;

            for (i8 i = 1; addMoveToList(p, column + i, row + i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column + i, row - i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column - i, row + i, alpha, beta, possibleMoves); ++i);
            for (i8 i = 1; addMoveToList(p, column - i, row - i, alpha, beta, possibleMoves); ++i);
        } break;
        case Rook:
        {
            //TODO castling how to?

            //bool castleLeftBackup;
            //bool castleRightBackup;

            //if (initialRow(p) == row)
            //{
            //    if (column == 0)
            //    {
            //        bool& canICastleLeft = board.canCastleLeft[(pieceColor(p) + 1) / 2];
            //        castleLeftBackup = canICastleLeft;
            //        canICastleLeft = false;
            //    }
            //    else if (column == 7)
            //    {
            //        bool& canICastleRight = board.canCastleRight[(pieceColor(p) + 1) / 2];
            //        castleRightBackup = canICastleRight;
            //        canICastleRight = false;
            //    }
            //}

            //for (i8 i = 1; addMoveToList(p, board, column + i, row, alpha, beta, possibleMoves); ++i);
            //for (i8 i = 1; addMoveToList(p, board, column - i, row, alpha, beta, possibleMoves); ++i);
            //for (i8 i = 1; addMoveToList(p, board, column, row + i, alpha, beta, possibleMoves); ++i);
            //for (i8 i = 1; addMoveToList(p, board, column, row - i, alpha, beta, possibleMoves); ++i);


            //if (initialRow(p) == row)
            //{
            //    if (column == 0)
            //    {
            //        bool& canICastleLeft = board.canCastleLeft[(pieceColor(p) + 1) / 2];
            //        canICastleLeft = castleLeftBackup;
            //    }
            //    else if (column == 7)
            //    {
            //        bool& canICastleRight = board.canCastleRight[(pieceColor(p) + 1) / 2];
            //        canICastleRight = castleRightBackup;
            //    }
            //}
        } break;
        case Queen:
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
        case King:
        {
            //TODO castling
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

        for (const auto& i : possibleMoves)
            tryPlacingPieceAt(p, i.second.first, i.second.second, depth - 1, alpha, beta, bestValue, doNotContinue, valueSoFar);


        //board.playerOnMove = oppositeSide(board.playerOnMove);
        board.pieceAt(column, row) = p;

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
float lastReportedLowerBound;
void printLowerBound()
{
    static std::mutex m;
    std::unique_lock lock(m);

    float currentLowerBound = alphaOrBeta;
    if (currentLowerBound*onMoveW < lastReportedLowerBound * onMoveW)
    {
        std::osyncstream syncout(out);
        syncout << "info " << "depth " << depthW + 1 << ' ';
        printScore(syncout, currentLowerBound, depthW + 1, oppositeSide(onMoveW));
        syncout << ' ';
        syncout << "lowerbound";
        syncout << nl << std::flush;
        lastReportedLowerBound = currentLowerBound;
    }

}

std::atomic<size_t> totalNodesDepth;
size_t totalNodesAll;

void evaluateGameMove(Variation& board, i8 depth)//, double alpha = -std::numeric_limits<float>::max(), double beta = std::numeric_limits<float>::max())
{
    //auto timeStart = std::chrono::high_resolution_clock::now();
    Variation localBoard(board);

    if (options.MultiPV > 1) [[unlikely]]//If we want to know multiple good moves, we cannot prune using a/B at root level
    {
        board.bestFoundValue = localBoard.bestMoveScore(depth, board.startingValue, -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
        //board.researchedTime = std::chrono::high_resolution_clock::now() - timeStart;
    }

    else
    {
        switch (localBoard.board.playerOnMove)
        {
        case PlayerSide::BLACK: {
            board.bestFoundValue = localBoard.bestMoveScore(depth, board.startingValue, alphaOrBeta, std::numeric_limits<float>::infinity());
            update_max(alphaOrBeta, board.bestFoundValue);
            //alphaOrBeta = std::max(alphaOrBeta * 1.0, researchedBoard->bestFoundValue * 1.0);
        } break;
        case PlayerSide::WHITE: {
            board.bestFoundValue = localBoard.bestMoveScore(depth, board.startingValue, -std::numeric_limits<float>::infinity(), alphaOrBeta);
            update_min(alphaOrBeta, board.bestFoundValue);
            //alphaOrBeta = std::min(alphaOrBeta * 1.0, researchedBoard->bestFoundValue * 1.0);
        } break;
        default:
            std::unreachable();
        }
        //board.researchedTime = std::chrono::high_resolution_clock::now() - timeStart;
        printLowerBound();
    }

    totalNodesDepth += localBoard.nodes;
}


static std::vector<Variation*> q;
static std::atomic<size_t> qPos;

void evaluateGameMoveFromQ(size_t posInQ, i8 depth)
{
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

void workerFromQ(size_t threadId)//, double alpha = -std::numeric_limits<float>::max(), double beta = std::numeric_limits<float>::max())
{
    while (true)
    {
        if (itsTimeToStop) [[unlikely]]
            return;
        size_t localPos = qPos++;
        if (localPos >= q.size()) [[unlikely]]//Stopper
            {
                --qPos;
                return;
            }
            
        auto& tmp = *q[localPos];

        AssertAssume(tmp.board.playerOnMove == onMoveW);

        evaluateGameMoveFromQ(localPos, depthW);
    }
}

void cutoffBadMoves(std::vector<Variation>& boards)
{
    float bestMoveScore = boards.front().bestFoundValue;

    float cutoffPoint = bestMoveScore + 50;

    size_t i = 1;
    for (i < boards.size(); ++i;)
    {
        if (boards[i].bestFoundValue > cutoffPoint)
            break;
    }

    std::cerr << "Cutting off " << boards.size() - i << " bad moves to save time" << std::endl;

    boards.resize(i);
}




void printMoveInfo(unsigned depth, const std::chrono::duration<float, std::milli>& elapsedTotal, const std::chrono::duration<float, std::milli> &elapsedDepth, const size_t& nodesDepth, const Variation& move, size_t moveRank, bool upperbound, PlayerSide pov)
{
    //auto elapsedTotal = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - timeGlobalStarted);
    out
        << "info "
        << "depth " << unsigned(depth) << ' '
        << "time "  << (size_t)round(elapsedTotal.count()) << ' '
        << "nodes " << totalNodesAll << ' '
        << "nps " << (size_t)round((nodesDepth / elapsedDepth.count())*1000)<< ' '
        ;
    printScore(out, move.bestFoundValue, depth, pov) << ' ';
    if (upperbound)
        out << "upperbound ";
    out
        << "multipv " << moveRank << ' '
        << "pv " << move.firstMoveNotation.data()
        << nl;
}

std::chrono::duration<float, std::milli> findBestOnSameLevel(std::vector<Variation>& boards, i8 depth)//, PlayerSide onMove)
{
    AssertAssume(!boards.empty());
    PlayerSide onMoveResearched = boards.front().board.playerOnMove;
    depth -= boards.front().variationDepth;
    totalNodesDepth = 0;

    //Check if all boards are from the same POV. Required for the a/B to work.
    for (const auto& i : boards)
        assert(i.board.playerOnMove == onMoveResearched);

#ifdef _DEBUG
    const size_t threadCount = 1;
#else
    const size_t threadCount = std::thread::hardware_concurrency() / 2;
#endif

    auto timeThisStarted = std::chrono::high_resolution_clock::now();
    if (depth > 0)
    {
        alphaOrBeta = std::numeric_limits<float>::max() * onMoveResearched;
        lastReportedLowerBound = alphaOrBeta;
        //transpositions.clear();

        itsTimeToStop = false;
        onMoveW = onMoveResearched;
        depthW = depth;
        q.clear();
        qPos = 0;

        q.reserve(boards.size());
        for (auto& board : boards)
            q.push_back(&board);

        //if(options.MultiPV<=1)
        //    evaluateGameMoveFromQ(qPos++, depthW);//It is usefull to run first pass on single core at full speed to set up alpha/Beta
        

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
    totalNodesAll += totalNodesDepth;

    if (qPos != boards.size())
    {
        std::cerr << "Not enough time to search all moves. Only managed to fully go through " << qPos << " out of " << boards.size() << std::endl;
    }

    boards.resize(qPos);

    switch (onMoveResearched)
    {
    case PlayerSide::WHITE: {
        std::stable_sort(boards.begin(), boards.end(), std::less<Variation>());
    } break;
    case PlayerSide::BLACK: {
        std::stable_sort(boards.begin(), boards.end(), std::greater<Variation>());
    } break;
    default:
        std::unreachable();
    }

    auto elapsedThis = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - timeThisStarted);
    auto elapsedTotal = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - timeGlobalStarted);


    //float scoreCp = boards.front().bestFoundValue;
    //std::cerr << "ScoreCP: " << scoreCp << std::endl;

    //if (itsTimeToStop)
    //    return;


    for (size_t i = boards.size()-1; i != 0; --i)
        printMoveInfo(depth + 1, elapsedTotal, elapsedThis, totalNodesDepth, boards[i], i + 1, options.MultiPV == 1, oppositeSide(onMoveW));

    printMoveInfo(depth + 1, elapsedTotal, elapsedThis, totalNodesDepth, boards.front(), 1, false, oppositeSide(onMoveW));

    std::cout << std::flush;

    return elapsedThis;
}


void timeLimit(int milliseconds, bool * doNotStop)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    if (!*doNotStop)
        itsTimeToStop = true;
    delete doNotStop;
    
}


std::vector<Variation> generateMoves(Board& board, PlayerSide bestForWhichSide)//, i8 depth = 1
{
    static auto rd = std::random_device{};
    static auto rng = std::default_random_engine{ rd() };
    alphaOrBeta = std::numeric_limits<float>::max() * board.playerOnMove;
    const i8 depth = 1;
    itsTimeToStop = false;
    onMoveW = board.playerOnMove;
    depthW = 1;
    totalNodesDepth = 0;
    totalNodesAll = 0;
    //saveToVector = true;

    out << "info depth 1" << nl << std::flush;
    //transpositions.clear();
    Variation tmp(board, 0, 0, 1, board.playerOnMove, std::array<char,6>{ '\0' });
    tmp.saveToVector = true;
    tmp.bestMoveScore(1, 0, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    totalNodesDepth = tmp.nodes;
    totalNodesAll = totalNodesDepth;
    //transpositions.clear();
    std::vector<Variation> res;// = std::move(firstPositions);
    res.reserve(tmp.firstPositions.size());

    //Add move names
    for (auto& pos : tmp.firstPositions)
    {
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

    if (!deterministic)
    {
        std::shuffle(res.begin(), res.end(), rng);
    }

    switch (bestForWhichSide)
    {
    case PlayerSide::WHITE: {
        std::sort(res.begin(), res.end(), std::greater<Variation>());
    } break;
    case PlayerSide::BLACK: {
        std::sort(res.begin(), res.end(), std::less<Variation>());
    } break;
    default:
        std::unreachable();
    }

    auto elapsedTotal = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - timeGlobalStarted);

    for (size_t i = res.size()-1;i!=(size_t)(-1); --i)
        printMoveInfo(depth, elapsedTotal, elapsedTotal, totalNodesDepth, res[i], i + 1, false, bestForWhichSide);

    std::cout << std::flush;

    return res;
}

std::pair<Board, float> findBestOnTopLevel(Board& board, i8 depth, PlayerSide onMove)
{
    auto tmp = generateMoves(board, onMove);//, onMove);
    findBestOnSameLevel(tmp, depth - 1);//, oppositeSide(onMove));
    const auto& res = tmp.front();
    return { res.board, res.bestFoundValue };
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
    board.movePiece(move[0], move[1], move[2], move[3]);

    //Castling
    if (move == "e1c1" && board.pieceAt(move[2], move[3]) == Piece::KingWhite)
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
        Piece evolvedInto = Piece::Nothing;
        if (move[3] == '8' && move[4] == 'q')
            evolvedInto = Piece::QueenWhite;
        else if (move[3] == '1' && move[4] == 'q')
            evolvedInto = Piece::QueenBlack;
        else if (move[3] == '8' && move[4] == 'r')
            evolvedInto = Piece::RookWhite;
        else if (move[3] == '1' && move[4] == 'r')
            evolvedInto = Piece::RookBlack;
        else if (move[3] == '8' && move[4] == 'b')
            evolvedInto = Piece::BishopWhite;
        else if (move[3] == '1' && move[4] == 'b')
            evolvedInto = Piece::BishopBlack;
        else if (move[3] == '8' && move[4] == 'k')
            evolvedInto = Piece::KnightWhite;
        else if (move[3] == '1' && move[4] == 'k')
            evolvedInto = Piece::KnightBlack;
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
            board.canCastleLeft[(PlayerSide::WHITE + 1) / 2] = false;
            board.canCastleRight[(PlayerSide::WHITE + 1) / 2] = false;
        } break;
        case('8'):
        {
            //Black moves king
            board.canCastleLeft[(PlayerSide::BLACK + 1) / 2] = false;
            board.canCastleRight[(PlayerSide::BLACK + 1) / 2] = false;
        } break;
        default:
            break;
        }
    } break;
    case('a'): {
        switch (move[1])
        {
        case('1'): {
            board.canCastleLeft[(PlayerSide::WHITE + 1) / 2] = false;//White moves left rook
        } break;
        case('8'): {
            board.canCastleLeft[(PlayerSide::BLACK + 1) / 2] = false;//Black moves left rook
        } break;
        default:
            break;
        }
    } break;
    case('h'): {
        switch (move[1])
        {
        case ('1'): {
            board.canCastleRight[(PlayerSide::WHITE + 1) / 2] = false;//White moves right rook
        } break;
        case ('8'): {
            board.canCastleRight[(PlayerSide::BLACK + 1) / 2] = false;//Black moves right rook
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

void parseMoves(Board& board, std::string_view str)
{
    if (getWord(str) != "moves")
        return;

    while (!str.empty())
    {
        executeMove(board, str);
    }
}



Board posFromString(std::string_view str)
{
    if (getWord(str) == "startpos")
    {
        Board res = Board::startingPosition();
        parseMoves(res, str);

#ifdef _DEBUG
        res.print(std::cerr);
#endif
        return res;
    }
    throw;//not implemented

}


Variation findBestInTimeLimit(Board& board, int milliseconds, bool endSooner = true)
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

    Variation res;
    //dynamicPositionRanking = true;

    std::wcout << "Depth: ";
    for (i8 i = 4; i < 100; i += 2) {
        auto bestPosFound = findBestOnSameLevel(boardList, i);//, oppositeSide(researchedBoard.playerOnMove));
        //dynamicPositionRanking = false;
        if (itsTimeToStop)
        {
            limit.join();
            break;
        }

        res = boardList.front();
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

Variation findBestInNumberOfMoves(Board& board, i8 moves)
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

    Variation res;
    //dynamicPositionRanking = true;

    std::wcout << "Depth: ";
    for (i8 i = 4; i <= moves; i += 2) {
        auto bestPosFound = findBestOnSameLevel(boardList, i);
        //dynamicPositionRanking = false;
        if (itsTimeToStop)
            break;
        else
        {
            res = boardList.front();
        }
        std::wcout << i << ' ';
    }
    std::wcout << std::endl;

    return res;
}



int uci(std::istream& in)
{
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
        std::getline(in, command);
        
        if (!in.good())
        {
            std::cerr << "End of input stream, rude! End the uci session with 'quit' in a controlled way." << std::endl;
            return -1;
        }
            


        std::string_view commandView(command);
        auto commandFirst = getWord(commandView);
        if (commandFirst == "uci")
        {
            out << "id name Klara Destroyer" << nl
                << "id author Matej Kocourek" << nl
                << "option name MultiPV type spin min 1 default " << options.MultiPV << nl
                << "uciok" << nl
                << std::flush;
        }
        else if (commandFirst == "isready")
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
            if (getWord(commandView) != "value")
                continue;
            auto optionValue = getWord(commandView);
            if (optionName == "MultiPV")
            {
                //std::cerr << "Option value was: " << optionValue << std::endl;
                options.MultiPV = std::atoll(optionValue.data());
                std::cerr << "Setting MultiPV to " << options.MultiPV << std::endl;
            }
        }
        else if (commandFirst == "go")
        {
            timeGlobalStarted = std::chrono::high_resolution_clock::now();

            typedef std::chrono::duration<double, std::milli> floatMillis;
            std::array<floatMillis, 2> playerTime;
            std::array<floatMillis, 2> playerInc;
            //int64_t wtime = 0, btime = 0, winc = 0, binc = 0;
            floatMillis timeTargetMax(0);
            floatMillis timeTargetOptimal(0);
            i8 maxDepth = std::numeric_limits<i8>::max();

            while (true)
            {
                auto word = getWord(commandView);
                if (word.empty())
                    break;
                else if (word == "wtime")
                    playerTime[(PlayerSide::WHITE + 1) / 2] = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                else if (word == "btime")
                    playerTime[(PlayerSide::BLACK + 1) / 2] = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                else if (word == "winc")
                    playerInc[(PlayerSide::WHITE + 1) / 2] = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                else if (word == "binc")
                    playerInc[(PlayerSide::BLACK + 1) / 2] = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                else if (word == "movetime")
                {
                    timeTargetMax = std::chrono::milliseconds(atoll(getWord(commandView).data()));
                    timeTargetOptimal = timeTargetMax;
                }
                else if (word == "infinite")
                {
                    timeTargetMax = floatMillis(std::numeric_limits<double>::infinity());
                    timeTargetOptimal = timeTargetMax;
                }
                else if (word == "depth")
                    maxDepth = atoll(getWord(commandView).data());
            }
            
            if (timeTargetMax == floatMillis(0))//We have to calculate our own time
            {
                float gamePhase = (17.0f - board.countPiecesMin()) / 16.0f;

                const auto& myTime = playerTime[(board.playerOnMove + 1) / 2];// == PlayerSide::WHITE ? wtime : btime;
                const auto& myInc = playerTime[(board.playerOnMove + 1) / 2];// == PlayerSide::WHITE ? winc : binc;

                timeTargetMax = floatMillis(myInc + myTime * gamePhase / 3);
                timeTargetOptimal = timeTargetMax / 3;
            }


            std::cerr << "Targeting " << timeTargetOptimal.count() << " ms." << std::endl;
            std::cerr << "Highest I can go is " << timeTargetMax.count() << " ms." << std::endl;

            itsTimeToStop = false;
            std::mutex mTimeout;
            std::condition_variable cvTimeout;

            if (timeTargetMax != floatMillis(std::numeric_limits<double>::infinity()))
            {
                std::thread threadTimeout([&cvTimeout, &mTimeout, timeTargetMax]()
                    {
                        std::unique_lock<std::mutex> l(mTimeout);
                        if (cvTimeout.wait_for(l, timeTargetMax) == std::cv_status::timeout)
                        {
                            itsTimeToStop = true;
                            std::cerr << "Timeout!" << std::endl;
                        }
                            
                    });

                threadTimeout.detach();
            }



            //dynamicPositionRanking = false;
            auto boardList = generateMoves(board,board.playerOnMove);
            //dynamicPositionRanking = true;

            Variation bestPosFound;

            std::vector<std::pair<double, float>> previousResults;

            //std::cerr << "Depth: ";

            for (i8 i = 2; i <= maxDepth; i += 2) {
                out << "info " << "depth " << unsigned(i) << nl << std::flush;

                size_t availableMoveCount = boardList.size();
                floatMillis elapsedThisDepth = findBestOnSameLevel(boardList, i);
                bool searchedThroughAllMoves = boardList.size() == availableMoveCount;


                bestPosFound = boardList.front();

                auto elapsedTotal = floatMillis(std::chrono::high_resolution_clock::now() - timeGlobalStarted);

                float scoreCp = bestPosFound.bestFoundValue;



                //dynamicPositionRanking = false;

                //res = bestPosFound;
                std::cerr << "Elapsed in this depth: " << elapsedThisDepth.count() << std::endl;
                //std::cerr << i + 1 << ' ';

                if (round(abs(scoreCp) / matePrice) > 0)
                {
                    std::cerr << "Mate possibility, no need to search further" << std::endl;
                    break;
                }
                    
                //std::cerr << " is " << elapsed << std::endl;

                if (elapsedTotal >= timeTargetMax)//Emergency stop if we depleted time
                {
                    std::cerr << "Emergency stop!" << std::endl;
                    break;
                }
                    
                previousResults.emplace_back(elapsedThisDepth.count(), bestPosFound.bestFoundValue);

                if (previousResults.size() >= 3)
                {
                    double logOlder = log2(previousResults[previousResults.size() - 2].first);
                    double logNewer = log2(previousResults[previousResults.size() - 1].first);
                    double growth = logNewer + logNewer - logOlder;

                    constexpr double branchingFactor = 1.2;
                    growth *= branchingFactor;

                    //double growth = pow(log2(previousResults[previousResults.size() - 1].first),2) / log2(previousResults[previousResults.size() - 2].first);
                    //std::cerr << "log is " << growth << std::endl;
                    auto projectedNextTime = floatMillis(pow(2, growth));
                    std::cerr << "Projected next time is " << projectedNextTime.count() << " ms." << std::endl;

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

                    if (diff < 100)//The difference is too small, we probably wouldn't get much further info.
                    {
                        std::cerr << "We could start another depth, but it's probably not necessary." << std::endl;
                        break;
                    }
                    std::cerr << "This time, we are using some extra time to really think about this move." << std::endl;
                }
            }
            cvTimeout.notify_one();//Cancel the timeout timer
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


void benchmark(i8 depth, Board board)
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

auto benchmark(size_t depth)
{
    std::stringstream ss;
    ss << "uci\nisready\nposition startpos\ngo infinite depth " << depth << "\nquit\n";
    std::cerr << ss.str() << std::endl;
    return uci(ss);
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
                promotion.pieceAt('h', '5') = Piece::PawnBlack;
                promotion.pieceAt('d', '5') = Piece::PawnBlack;
                promotion.pieceAt('f', '5') = Piece::KingBlack;
                promotion.pieceAt('g', '1') = Piece::BishopBlack;
                promotion.pieceAt('e', '2') = Piece::KingWhite;
                promotion.pieceAt('b', '5') = Piece::PawnWhite;
                promotion.pieceAt('a', '6') = Piece::PawnWhite;
                promotion.playerOnMove = PlayerSide::WHITE;

                promotion.print();

                benchmark(8, promotion);
            } break;
            case (2):
            {
                Board testMatu;
                testMatu.pieceAt('h', '8') = Piece::KingBlack;
                testMatu.pieceAt('a', '1') = Piece::KingWhite;
                testMatu.pieceAt('g', '1') = Piece::RookWhite;
                testMatu.pieceAt('a', '7') = Piece::RookWhite;
                testMatu.pieceAt('b', '1') = Piece::QueenWhite;
                testMatu.pieceAt('c', '7') = Piece::PawnWhite;
                testMatu.playerOnMove = PlayerSide::WHITE;
                testMatu.print();

                benchmark(8, testMatu);
            } break;
            case (3):
            {
                Board testMatu;
                testMatu.pieceAt('h', '8') = Piece::KingBlack;
                testMatu.pieceAt('h', '7') = Piece::PawnWhite;
                testMatu.pieceAt('g', '6') = Piece::PawnWhite;
                testMatu.pieceAt('h', '6') = Piece::KingWhite;
                testMatu.pieceAt('h', '5') = Piece::PawnWhite;
                testMatu.pieceAt('g', '5') = Piece::PawnWhite;

                //testMatu.deleteAndOverwritePiece('h', '4', &kingWhite);
                //testMatu.deleteAndOverwritePiece('h', '3', &pawnWhite);
                //testMatu.deleteAndOverwritePiece('g', '3', &pawnWhite);
                //testMatu.deleteAndOverwritePiece('g', '4', &pawnWhite);

                testMatu.playerOnMove = PlayerSide::WHITE;
                testMatu.print();

                benchmark(8, testMatu);
            } break;
            case (4):
            {
                Board test = Board::startingPosition();
                //constexpr auto tmp = test.piecesCount();

                test.pieceAt('b', '1') = Piece::Nothing;
                test.pieceAt('c', '1') = Piece::Nothing;
                test.pieceAt('d', '1') = Piece::Nothing;
                test.pieceAt('f', '1') = Piece::Nothing;
                test.pieceAt('g', '1') = Piece::Nothing;

                test.playerOnMove = PlayerSide::WHITE;
                test.print();

                benchmark(8, test);
            } break;
            case (5):
            {
                constexpr auto test = Board::startingPosition();
                constexpr auto balance = test.balance();
                constexpr auto counter = test.piecesCount();
                constexpr auto min = test.countPiecesMin();
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
        return uci(std::cin);
    }
        

    return 0;
}