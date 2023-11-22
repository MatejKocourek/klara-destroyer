﻿#ifndef MS_STDLIB_BUGS // Allow overriding the autodetection.
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
    //NONE = 0
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
    return (PlayerSide)((-1) * (i8)side);
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
    TempSwap(T& toSwap, T&& tempNewValue) : toSwap(toSwap), backup(toSwap)
    {
        toSwap = std::forward<T>(tempNewValue);
    }
    ~TempSwap()
    {
        toSwap = backup;
    }
};

struct Piece {
    virtual constexpr wchar_t symbolW() const = 0;

    virtual constexpr PlayerSide occupancy() const = 0;

    //virtual constexpr i8 initialRow() const = 0;

    constexpr float priceAbsolute(i8 column, i8 row) const {
        auto res = pricePiece() + priceAdjustmentPov(column, row);
        AssertAssume(res >= 0);
        return res;
    }

    constexpr float priceRelative(i8 column, i8 row) const {
        return priceAbsolute(column, row) * occupancy();
    }

    virtual constexpr float pricePiece() const = 0;

    virtual constexpr float priceAdjustment(i8 column, i8 row) const = 0;

    virtual constexpr float priceAdjustmentPov(i8 column, i8 row) const = 0;

    virtual constexpr char symbolA() const = 0;

    std::ostream& print(std::ostream& os) const {
        return os << symbolA();
    }
    std::wostream& print(std::wostream& os) const {
        return os << symbolW();
    }

    virtual float bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNotContinue = false) const = 0;

    virtual float bestMoveWithThisPieceScoreOrdered(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNotContinue = false) const
    {
        return bestMoveWithThisPieceScore(board, column, row, depth, alpha, beta, valueSoFar, doNotContinue);
    }

    template <typename T>
    bool addMoveToList(Board& board, i8 column, i8 row, float alpha, float beta, T& possibleMoves) const;

    void placePieceAt(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, bool& doNotContinue, float valueSoFar, float price) const;

    template <typename F>
    bool tryPlacingPieceAt(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, bool& doNotContinue, float valueSoFar, F condition) const;

    auto tryPlacingPieceAt(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, bool& doNotContinue, float valueSoFar) const
    {
        return tryPlacingPieceAt(board, column, row, depth, alpha, beta, bestValue, doNotContinue, valueSoFar, MOVE_PIECE_FREE_CAPTURE);
    }

    virtual ~Piece() = default;
};

//struct Bait : public Piece
//{
//    constexpr wchar_t symbolW() const final
//    {
//        return L'?';
//    }
//    constexpr char symbolA() const final
//    {
//        return '?';
//    }
//    constexpr float pricePiece() const final
//    {
//        return std::numeric_limits<float>::max();
//    }
//    constexpr float priceAdjustment(i8 column, i8 row) const final
//    {
//        return 0;
//    }
//    constexpr float priceAdjustmentPov(i8 column, i8 row) const final
//    {
//        return 0;
//    }
//    float bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNotContinue = false) const final
//    {
//        return std::numeric_limits<float>::infinity() * (-1) * occupancy();
//    }
//};


struct WhitePiece : virtual Piece {
    constexpr PlayerSide occupancy() const final {
        return PlayerSide::WHITE;
    }
    constexpr float priceAdjustmentPov(i8 column, i8 row) const final {
        return priceAdjustment(column, 7 - row);
    }
};
struct BlackPiece : virtual Piece {
    constexpr PlayerSide occupancy() const final {
        return PlayerSide::BLACK;
    }
    constexpr float priceAdjustmentPov(i8 column, i8 row) const final {
        return priceAdjustment(column, row);
    }
};

//struct BaitBlack final : public Bait
//{
//    constexpr PlayerSide occupancy() const final
//    {
//        return PlayerSide::BLACK;
//    }
//    //constexpr i8 initialRow() const final
//    //{
//    //    throw;
//    //}
//} baitBlack;
//
//struct BaitWhite final : public Bait
//{
//    constexpr PlayerSide occupancy() const final
//    {
//        return PlayerSide::WHITE;
//    }
//    //constexpr i8 initialRow() const final
//    //{
//    //    throw;
//    //}
//} baitWhite;



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
    std::array<const Piece*, 64> pieces;
    PlayerSide playerOnMove;
    std::array<bool, 2> canCastleLeft;
    std::array<bool, 2> canCastleRight;

    auto operator<=>(const Board&) const noexcept = default;

    Board(const Board& copy) = default;

    Board() : pieces{ {nullptr} }
    {
        //for (auto& i : pieces) {
        //    i = nullptr;
        //}
    }

    Board& operator=(const Board& copy) = default;

    std::array<i8, 128> piecesCount() const
    {
        std::array<i8, 128> res { 0 };
        for (const auto& i : pieces)
        {
            if (i != nullptr)
                ++res[i->symbolA()];
        }
        return res;
    }

    bool canTakeKing(PlayerSide onMove)
    {
        i32 totalMoves = 0;
        constexpr float valueSoFar = 0;

        float alpha, beta;
        alpha = -std::numeric_limits<float>::max();
        beta = std::numeric_limits<float>::max();

        auto backup = playerOnMove;
        playerOnMove = onMove;

        bool backupSave = saveToVector;
        saveToVector = false;
        auto backupCastlingLeft = canCastleLeft;
        auto backupCastlingRight = canCastleRight;

        canCastleLeft.fill(false);
        canCastleRight.fill(false);

        for (i8 i = 0; i < pieces.size(); ++i) {
            const Piece* found = pieces[i];

            if (found == nullptr)
                continue;
            if (found->occupancy() == onMove)
            {
                auto foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, valueSoFar);

                if (foundVal * onMove == kingPrice) [[unlikely]]//Je možné vzít krále, hra skončila
                {
                    canCastleLeft = backupCastlingLeft;
                    canCastleRight = backupCastlingRight;
                    saveToVector = backupSave;
                    playerOnMove = backup;

                    return true;
                }
                
            }
        }

        canCastleLeft = backupCastlingLeft;
        canCastleRight = backupCastlingRight;
        saveToVector = backupSave;

        playerOnMove = backup;
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
    //        if (found->occupancy() == attacker)
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
        i32 totalMoves = 0;
        constexpr float valueSoFar = 0;

        float alpha, beta;
        alpha = -std::numeric_limits<float>::max();
        beta = std::numeric_limits<float>::max();

        for (i8 i = 0; i < pieces.size(); ++i) {
            const Piece* found = pieces[i];

            if (found == nullptr)
                continue;
            if (found->occupancy() == onMove)
            {
                auto foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, valueSoFar);

                if (foundVal != std::numeric_limits<float>::infinity() * (-1) * onMove)
                    return true;
            }
        }
        return false;
    }

    i32 possibleMovesCount(PlayerSide onMove)
    {
        i32 totalMoves = 0;
        constexpr float valueSoFar = 0;

        float alpha, beta;
        alpha = -std::numeric_limits<float>::max();
        beta = std::numeric_limits<float>::max();

        for (i8 i = 0; i < pieces.size(); ++i) {
            const Piece* found = pieces[i];

            if (found == nullptr)
                continue;
            if (found->occupancy() == onMove)
                auto foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, valueSoFar);
        }
        return totalMoves;
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

    float priceInLocation(i8 column, i8 row, PlayerSide playerColor) const
    {
        if (column < 0 || column > 7 || row < 0 || row > 7)
            return -std::numeric_limits<float>::infinity();

        const auto& piece = pieceAt(column,row);

        AssertAssume(playerColor == PlayerSide::BLACK || playerColor == PlayerSide::WHITE);

        if (piece == nullptr)
            return 0;
        else
            return piece->priceRelative(column, row) * (-playerColor);
    }

    const Piece*& pieceAt(i8 column, i8 row)
    {
        AssertAssume (!(column < 0 || column > 7 || row < 0 || row > 7));
        return pieces[column + (row * 8)];
    }
    const Piece* const& pieceAt(i8 column, i8 row) const
    {
        AssertAssume(!(column < 0 || column > 7 || row < 0 || row > 7));
        return pieces[column + (row * 8)];
    }

    const Piece*& pieceAt(char column, char row)
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            throw std::exception("Invalid coordinates");
        else
            return pieceAt((i8)(column - 'a'), (i8)(row - '1'));
    }
    const Piece* const& pieceAt(char column, char row) const
    {
        if (column < 'a' || column>'h' || row < '1' || row>'8')
            throw std::exception("Invalid coordinates");
        else
            return pieceAt((i8)(column - 'a'), (i8)(row - '1'));
    }

    void movePiece(char columnFrom, char rowFrom, char columnTo, char rowTo)
    {
        auto& from = pieceAt(columnFrom, rowFrom);
        auto& to = pieceAt(columnTo, rowTo);

        if (from == nullptr)
            throw std::exception("Trying to move an empty field");

        to = from;
        from = nullptr;
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

        const Piece* oldPiece = nullptr;

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
        double res = 0;
        for (i8 i = 0; i < 64; ++i) {
            if (pieces[i] != nullptr)// && pieces[i]->pricePiece()!=kingPrice)
                res += pieces[i]->priceRelative(i % 8, i / 8);
        }

        return res;
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
    //            pieces[i]->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), 1, alpha, beta, totalMoves, totalValues, 0,true);

    //            res += (((double)totalMoves)/2.0) * pieces[i]->occupancy();
    //            if(totalMoves>0)
    //                res += std::min(std::max(totalValues,-100.0),100.0);
    //        }
    //    }
    //    return res / 10000.0;
    //}


    //template <bool saveToVector>
    float bestMoveScore(i8 depth, float valueSoFar, float alpha, float beta)
    {
        AssertAssume(playerOnMove == 1 || playerOnMove == -1);

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


        float bestValue = std::numeric_limits<float>::infinity() * (-1) * playerOnMove;

        //if (itsTimeToStop) [[unlikely]]
        //    return bestValue;

        if (depth > depthToStopOrderingPieces) [[unlikely]]
        {
            static_vector<std::pair<float, i8>, 16> possiblePiecesToMove;

            for (i8 i = 0; i < 64; ++i) {
                float alphaTmp = alpha;
                float betaTmp = beta;
                const Piece* found = pieces[i];

                if (found == nullptr)
                    continue;
                if (found->occupancy() == playerOnMove)
                    possiblePiecesToMove.emplace_back(pieces[i]->bestMoveWithThisPieceScore(*this, i % 8, i / 8, 0, alphaTmp, betaTmp, valueSoFar), i);
            }

            switch (playerOnMove)
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
                const Piece* found = pieces[i];
                float foundVal;
                if (depth > depthToStopOrderingMoves) [[unlikely]]
                {
                    foundVal = found->bestMoveWithThisPieceScoreOrdered(*this, (i % 8), (i / 8), depth, alpha, beta, valueSoFar);
                }
                else
                {
                    foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), depth, alpha, beta, valueSoFar);
                }

                if (depth == depthW && options.MultiPV == 1) [[unlikely]]
                    {
                        //std::osyncstream(std::cerr) << "alpha: " << alpha << ", beta: " << beta << std::endl;
                        switch (playerOnMove)
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
        else [[likely]]
        {
            for (i8 i = 0; i < 64; ++i) {
                const Piece* found = pieces[i];

                if (found == nullptr)
                    continue;
                if (found->occupancy() == playerOnMove)
                {
                    auto foundVal = found->bestMoveWithThisPieceScore(*this, (i % 8), (i / 8), depth, alpha, beta, valueSoFar);

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
        }

        //if (saveToVector) [[unlikely]]
            //return -std::numeric_limits<float>::max();


        if (bestValue == (float)(std::numeric_limits<float>::infinity() * playerOnMove * (-1))) [[unlikely]]//Nemůžu udělat žádný legitimní tah (pat nebo mat)
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


    float tryPiece(i8 column, i8 row, const Piece* p, i8 depth, float alpha, float beta, float valueSoFar)
    {
        const Piece* backup = pieceAt(column, row);
        pieceAt(column, row) = p;
        auto foundVal = bestMoveScore(depth, valueSoFar, alpha, beta);
        pieceAt(column, row) = backup;
        return foundVal;
    }

    //double tryPiecePosition(i8 column, i8 row, const Piece* p)
    //{
    //    const Piece* backup = pieceAt(column, row);
    //    pieceAt(column, row) = p;
    //    auto foundVal = positionScoreHeuristic();
    //    pieceAt(column, row) = backup;
    //    return foundVal;
    //}

    i8 countPiecesMin() const
    {
        std::array<i8, 2> counters{ 0 };
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



struct Knight : virtual public Piece {
    virtual float bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;
    virtual float bestMoveWithThisPieceScoreOrdered(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;

    virtual constexpr float pricePiece() const final {
        //return 320;
        return 337;
    }

    constexpr float priceAdjustment(i8 column, i8 row) const final
    {
        AssertAssume(column < 8 && row < 8 && column >=0 && row >= 0);
        //constexpr std::array<std::array<float, 8>, 8> arr = { {
        //    {-50, -40, -30, -30, -30, -30, -40, -50},
        //    {-40, -20, + 0, + 0, + 0, + 0, -20, -40},
        //    {-30, + 0, +10, +15, +15, +10, + 0, -30},
        //    {-30, + 5, +15, +20, +20, +15, + 5, -30},
        //    {-30, + 0, +15, +20, +20, +15, + 0, -30},
        //    {-30, + 5, +10, +15, +15, +10, + 5, -30},
        //    {-40, -20, + 0, + 5, + 5, + 0, -20, -40},
        //    {-50, -40, -30, -30, -30, -30, -40, -50},
        //} };
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

        return arr[row*8 + column];
    }
};


struct KnightWhite final :public Knight, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♘';
    }
    constexpr char symbolA() const override {
        return 'n';
    }
} knightWhite;

struct KnightBlack final :public Knight, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♞';
    }
    constexpr char symbolA() const override {
        return 'N';
    }
} knightBlack;

struct Bishop : virtual public Piece {
    virtual float bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;
    virtual float bestMoveWithThisPieceScoreOrdered(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;

    virtual constexpr float pricePiece() const final {
        //return 333;
        return 365;
    }

    constexpr float priceAdjustment(i8 column, i8 row) const final
    {
        AssertAssume(column < 8 && row < 8);
        //constexpr std::array<std::array<float, 8>, 8> arr = { {
        //    {-20, -10, -10, -10, -10, -10, -10, -20},
        //    {-10, + 0, + 0, + 0, + 0, + 0, + 0, -10},
        //    {-10, + 0, + 5, +10, +10, + 5, + 0, -10},
        //    {-10, + 5, + 5, +10, +10, + 5, + 5, -10},
        //    {-10, + 0, +10, +10, +10, +10, + 0, -10},
        //    {-10, +10, +10, +10, +10, +10, +10, -10},
        //    {-10, + 5, + 0, + 0, + 0, + 0, + 5, -10},
        //    {-20, -10, -10, -10, -10, -10, -10, -20},
        //} };

        //return arr[row][column];

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

        return arr[row * 8 + column];
    }
};
struct BishopWhite final :public Bishop, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♗';
    }
    constexpr char symbolA() const override {
        return 'b';
    }
} bishopWhite;

struct BishopBlack final :public Bishop, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♝';
    }
    constexpr char symbolA() const override {
        return 'B';
    }
} bishopBlack;



struct Rook : virtual public Piece {
    virtual float bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;
    virtual float bestMoveWithThisPieceScoreOrdered(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;

    virtual constexpr float pricePiece() const final {
        //return 510;
        return 477;
    }

    constexpr float priceAdjustment(i8 column, i8 row) const final
    {
        AssertAssume(column < 8 && row < 8);
        //constexpr std::array<std::array<float, 8>, 8> arr = { {
        //    {+ 0, + 0, + 0, + 0, + 0, + 0, + 0, + 0},
        //    {+ 5, +10, +10, +10, +10, +10, +10, + 5},
        //    {- 5, + 0, + 0, + 0, + 0, + 0, + 0, - 5},
        //    {- 5, + 0, + 0, + 0, + 0, + 0, + 0, - 5},
        //    {- 5, + 0, + 0, + 0, + 0, + 0, + 0, - 5},
        //    {- 5, + 0, + 0, + 0, + 0, + 0, + 0, - 5},
        //    {- 5, + 0, + 0, + 0, + 0, + 0, + 0, - 5},
        //    {+ 0, + 0, + 0, + 5, + 5, + 0, + 0, + 0},
        //} };

        //return arr[row][column];

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

        return arr[row * 8 + column];
    }

    virtual constexpr bool initialRow() const = 0;
};

struct RookWhite final :public Rook, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♖';
    }
    constexpr char symbolA() const override {
        return 'r';
    }

    virtual constexpr bool initialRow() const {
        return 0;
    }
} rookWhite;

struct RookBlack final :public Rook, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♜';
    }
    constexpr char symbolA() const override {
        return 'R';
    }

    virtual constexpr bool initialRow() const {
        return 7;
    }
} rookBlack;


struct Queen : virtual public Piece {

    virtual float bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;

    virtual float bestMoveWithThisPieceScoreOrdered(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;

    virtual constexpr float pricePiece() const final {
        //return 880;
        return 1025;
    }

    constexpr float priceAdjustment(i8 column, i8 row) const final
    {
        AssertAssume(column < 8 && row < 8);
        //constexpr std::array<std::array<float, 8>, 8> arr = { {
        //    {-20, -10, -10, - 5, - 5, -10, -10, -20},
        //    {-10, + 0, + 0, + 0, + 0, + 0, + 0, -10},
        //    {-10, + 0, + 5, + 5, + 5, + 5, + 0, -10},
        //    {- 5, + 0, + 5, + 5, + 5, + 5, + 0, - 5},
        //    {+ 0, + 0, + 5, + 5, + 5, + 5, + 0, - 5},
        //    {-10, + 5, + 5, + 5, + 5, + 5, + 0, -10},
        //    {-10, + 0, + 5, + 0, + 0, + 0, + 0, -10},
        //    {-20, -10, -10, - 5, - 5, -10, -10, -20},
        //} };

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

        return arr[row * 8 + column];
    }
};
struct QueenWhite final :public Queen, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♕';
    }
    constexpr char symbolA() const override {
        return 'q';
    }
} queenWhite;
struct QueenBlack final :public Queen, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♛';
    }
    constexpr char symbolA() const override {
        return 'Q';
    }
} queenBlack;


struct King : virtual public Piece {
    virtual float bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;

    virtual constexpr float pricePiece() const final {
        return kingPrice;
    }

    constexpr float priceAdjustment(i8 column, i8 row) const final
    {
        AssertAssume(column < 8 && row < 8);
        //constexpr std::array<std::array<float, 8>, 8> arr = { {
        //    {-30, -40, -40, -50, -50, -40, -40, -30},
        //    {-30, -40, -40, -50, -50, -40, -40, -30},
        //    {-30, -40, -40, -50, -50, -40, -40, -30},
        //    {-30, -40, -40, -50, -50, -40, -40, -30},
        //    {-20, -30, -30, -40, -40, -30, -30, -20},
        //    {-10, -20, -20, -20, -20, -20, -20, -10},
        //    {+20, +20, + 0, + 0, + 0, + 0, +20, +20},
        //    {+20, +30, +10, + 0, + 0, +10, +30, +20},
        //} };

        //return arr[row][column];

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

        return arr[row * 8 + column];
    }
private:
    template <i8 rookColumn, i8 newRookColumn>
    void tryCastling(Board& board, i8 row, /*i8 kingColumn, i8 rookColumn, i8 newRookColumn,*/ bool& canICastleLeft, bool& canICastleRight, float& bestValue, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const
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
            if (board.pieceAt(i, row) != nullptr) [[likely]]//The path is not vacant
                return;
        }

        auto pieceInCorner = dynamic_cast<const Rook*>(board.pieceAt(rookColumn, row));
        if (pieceInCorner == nullptr)//The piece in the corner is not a rook or is vacant
            return;

        if (saveToVector)[[unlikely]]//To optimize time, we do not check whether the path is attacked. May produce bad predictions, but only for small number of cases.
        {
            for (i8 i = kingColumn; i != newKingColumn; i -= sign)//Do not check the last field (where the king should be placed), it will be checked later anyway
            {
                TempSwap<const Piece*> fieldSwap(board.pieceAt(i, row), this);
                if (board.canTakeKing(oppositeSide(occupancy()))) [[unlikely]]//The path is attacked by enemy
                {
                     return;
                }
            }
        }

        //Now we can be sure we can do the castling
        valueSoFar -= pieceInCorner->priceAdjustmentPov(rookColumn, row);//Remove the position score of the rook, it is leaving
        valueSoFar += pieceInCorner->priceAdjustmentPov(newRookColumn, row);//Add the score of the rook on the next position

        //Castling is not allowed from this point onwards
        TempSwap castleLeftBackup(canICastleLeft, false);
        TempSwap castleRightBackup(canICastleRight, false);

        //Do the actual piece movement
        TempSwap<const Piece*> rookBackup(board.pieceAt(rookColumn, row), nullptr);
        TempSwap<const Piece*> newRookBackup(board.pieceAt(newRookColumn, row), pieceInCorner);
        tryPlacingPieceAt(board, newKingColumn, row, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
        //State will be restored when calling destructors
    }
};
struct KingWhite final :public King, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♔';
    }
    constexpr char symbolA() const override {
        return 'k';
    }
} kingWhite;
struct KingBlack final :public King, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♚';
    }
    constexpr char symbolA() const override {
        return 'K';
    }
} kingBlack;


struct Pawn : virtual public Piece {
    constexpr virtual i8 evolveRow() const = 0;
    //virtual static_vector<Piece*, 4>* evolveIntoReference(i8 row) const = 0;
    virtual const std::array<Piece*, 4>& availablePromotes() const = 0;
    constexpr virtual i8 advanceRow() const = 0;
    constexpr virtual bool canGoTwoFields(i8 row) const = 0;

    constexpr float pricePiece() const final
    {
        //return 100;
        return 82;
    }
    constexpr float priceAdjustment(i8 column, i8 row) const final
    {
        AssertAssume(column < 8 && row < 8);
        //constexpr std::array<std::array<float, 8>, 8> arr = { {
        //    {+ 0, + 0, + 0, + 0, + 0, + 0, + 0, + 0},
        //    {+50, +50, +50, +50, +50, +50, +50, +50},
        //    {+10, +10, +20, +30, +30, +20, +10, +10},
        //    {+ 5, + 5, +10, +25, +25, +10, + 5, + 5},
        //    {+ 0, + 0, + 0, +20, +20, + 0, + 0, + 0},
        //    {+ 5, - 5, -10, + 0, + 0, -10, - 5, + 5},
        //    {+ 5, +10, +10, -20, -20, +10, +10, + 5},
        //    {+ 0, + 0, + 0, + 0, + 0, + 0, + 0, + 0},
        //} };

        //return arr[row][column];

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

        return arr[row * 8 + column];
    }
    virtual float bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const override;
};

struct PawnWhite final :public Pawn, public WhitePiece {
    constexpr wchar_t symbolW() const override {
        return L'♙';
    }
    constexpr i8 evolveRow() const override {
        return 7;
    }

    constexpr bool canGoTwoFields(i8 row) const override {
        return row == 1;
    }

    constexpr i8 advanceRow() const override {
        return 1;
    }

    constexpr char symbolA() const override {
        return 'p';
    }

    //virtual static_vector<Piece*, 4>* evolveIntoReference(i8 row) const override
    //{
    //    static static_vector<Piece*, 4> whiteEvolvePawnOnly{ &pawnWhite };
    //    static static_vector<Piece*, 4> whiteEvolveLastRow{ &queenWhite, &rookWhite, &bishopWhite, &knightWhite };
    //    if (row == evolveRow()) [[unlikely]]
    //        return &whiteEvolveLastRow;
    //    else [[likely]]
    //        return &whiteEvolvePawnOnly;
    //}
    const std::array<Piece*, 4>& availablePromotes() const override
    {
        static std::array<Piece*, 4> whiteEvolveLastRow{ &queenWhite, &rookWhite, &bishopWhite, &knightWhite };
        return whiteEvolveLastRow;
    }

} pawnWhite;
struct PawnBlack final :public Pawn, public BlackPiece {
    constexpr wchar_t symbolW() const override {
        return L'♟';
    }
    constexpr i8 evolveRow() const override {
        return 0;
    }

    bool canGoTwoFields(i8 row) const override {
        return row == 6;
    }
    i8 advanceRow() const override {
        return -1;
    }

    constexpr char symbolA() const override {
        return 'P';
    }

    //virtual static_vector<Piece*, 4>* evolveIntoReference(i8 row) const override
    //{
    //    static static_vector<Piece*, 4> blackEvolvePawnOnly{ &pawnBlack };
    //    static static_vector<Piece*, 4> blackEvolveLastRow{ &queenBlack, &rookBlack, &bishopBlack, &knightBlack };
    //    if (row == evolveRow()) [[unlikely]]
    //        return &blackEvolveLastRow;
    //    else [[likely]]
    //        return &blackEvolvePawnOnly;
    //}

    const std::array<Piece*, 4>& availablePromotes() const override
    {
        static std::array<Piece*, 4> blackEvolveLastRow{ &queenBlack, &rookBlack, &bishopBlack, &knightBlack };
        return blackEvolveLastRow;
    }

} pawnBlack;




struct GameMove {
    Board researchedBoard;
    float bestFoundValue;
    float startingValue;
    //Board movePosition;
    i8 researchedDepth;
    //std::chrono::duration<float, std::milli> researchedTime{0};

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
    GameMove(Board board, float startingValue) :researchedBoard(std::move(board)), bestFoundValue(startingValue), startingValue(startingValue) {}
};


static std::vector<GameMove> firstPositions;


void Piece::placePieceAt(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, bool& doNotContinue, float valueSoFar, float priceTaken) const
{
    if (saveToVector && depth == 0) [[unlikely]]
    {
        TempSwap<const Piece*> backup(board.pieceAt(column, row), this);
        TempSwap saveVectorBackup(saveToVector, false);
        if (board.isValidSetup())
        {
            //float pieceTakenValue = valueSoFar + priceAbsolute * occupancy();
            //board.print();
            float balance = board.balance();
            firstPositions.emplace_back(board, balance);
        }
        return;
    }

    //if (doNotContinue) [[unlikely]]
    //    return;

    if (priceTaken >= kingPrice - 128) [[unlikely]]//Je možné vzít krále
    {
        doNotContinue = true;
        bestValue = kingPrice * occupancy();
        //totalMoves++;
        //return;
    }
    else [[likely]]
    {
        float valueGained = priceTaken + priceAdjustmentPov(column, row); //We are entering new position with this piece

        const auto wholeMovesFromRoot = ((depthW - depth) / 2);//Kolikaty tah od initial pozice (od 0), ne pultah
        valueGained *= (1 - (wholeMovesFromRoot * 0.0001));//10000.0);

        valueSoFar += valueGained * occupancy();//Add our gained value to the score

        float foundVal;
        if (depth > 0)
        {
            foundVal = board.tryPiece(column, row, this, depth, alpha, beta, valueSoFar);

            if ((foundVal * occupancy() * (-1)) == kingPrice)//V dalším tahu bych přišel o krále, není to legitimní tah
                return;
        }
        else//leaf node of the search tree
            foundVal = valueSoFar;

        if (foundVal * occupancy() > bestValue * occupancy())
            bestValue = foundVal;
    }

    switch (occupancy())
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
bool Piece::tryPlacingPieceAt(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float& bestValue, bool& doNotContinue, float valueSoFar, F condition) const
{
    if (doNotContinue)
        return false;

    float price = board.priceInLocation(column, row, occupancy());

    if (condition(price, 0))
    {
        placePieceAt(board, column, row, depth, alpha, beta, bestValue, doNotContinue, valueSoFar, price);
        return price==0;
    }
    else
        return false;
}

template <typename T>
bool Piece::addMoveToList(Board& board, i8 column, i8 row, float alpha, float beta, T& possibleMoves) const
{
    bool doNotContinueTmp = false;
    float bestValue = -std::numeric_limits<float>::infinity() * occupancy();
    bool fieldWasFree = tryPlacingPieceAt(board, column, row, 0, alpha, beta, bestValue, doNotContinueTmp, 0);

    if (bestValue != -std::numeric_limits<float>::infinity() * occupancy())
        possibleMoves.emplace_back(bestValue, std::make_pair(column, row));

    return fieldWasFree;
}


float Pawn::bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const
{
    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();


    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position

    board.playerOnMove = oppositeSide(board.playerOnMove);

    if (row == evolveRow()) [[unlikely]]
    {
        const auto& availableOptions = availablePromotes();//evolveIntoReference(row + advanceRow());
        for (const auto& evolveOption : availableOptions) {
            float valueDifferenceNextMove = (evolveOption->pricePiece() - pricePiece()) * occupancy();//Increase in material when the pawn promots
            float valueSoFarEvolved = valueSoFar + valueDifferenceNextMove;

            //Capture diagonally
            evolveOption->tryPlacingPieceAt(board, column - 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFarEvolved, MOVE_PIECE_CAPTURE_ONLY);
            evolveOption->tryPlacingPieceAt(board, column + 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFarEvolved, MOVE_PIECE_CAPTURE_ONLY);

            //Go forward
            if (board.pieceAt(column, row + advanceRow()) == nullptr) [[likely]]//Field in front of the pawn is empty, can step forward
            {
                evolveOption->tryPlacingPieceAt(board, column, row + advanceRow(), depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFarEvolved, MOVE_PIECE_FREE_ONLY);
            }
        }
    }
    else [[likely]]
    {
        //Capture diagonally
        tryPlacingPieceAt(board, column - 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar, MOVE_PIECE_CAPTURE_ONLY);
        tryPlacingPieceAt(board, column + 1, row + advanceRow(), depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar, MOVE_PIECE_CAPTURE_ONLY);

        //Go forward
        bool goForwardSuccessful = tryPlacingPieceAt(board, column, row + advanceRow(), depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar, MOVE_PIECE_FREE_ONLY);
        if (goForwardSuccessful) [[likely]]//Field in front of the pawn is empty, can make a second step
        {
            if (canGoTwoFields(row))//Two fields forward
            {
                tryPlacingPieceAt(board, column, row + advanceRow() * 2, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar, MOVE_PIECE_FREE_ONLY);
            }
        }
    }

    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;


    return bestValue;
}

float Knight::bestMoveWithThisPieceScoreOrdered(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const {
    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position
    board.playerOnMove = oppositeSide(board.playerOnMove);

    static_vector<std::pair<float, std::pair<i8, i8>>, 8> possibleMoves;


    addMoveToList(board, column + 1, row + 2, alpha, beta, possibleMoves);
    addMoveToList(board, column + 1, row - 2, alpha, beta, possibleMoves);
    addMoveToList(board, column + 2, row + 1, alpha, beta, possibleMoves);
    addMoveToList(board, column + 2, row - 1, alpha, beta, possibleMoves);
    addMoveToList(board, column - 1, row + 2, alpha, beta, possibleMoves);
    addMoveToList(board, column - 1, row - 2, alpha, beta, possibleMoves);
    addMoveToList(board, column - 2, row + 1, alpha, beta, possibleMoves);
    addMoveToList(board, column - 2, row - 1, alpha, beta, possibleMoves);

    switch (occupancy())
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


    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();

    for (const auto& i : possibleMoves)
        tryPlacingPieceAt(board, i.second.first, i.second.second, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);


    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;

    return bestValue;
}

float Knight::bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const {

    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();

    //if (depth <= 0)
      //  return 0;
    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position

    board.playerOnMove = oppositeSide(board.playerOnMove);

    tryPlacingPieceAt(board, column + 1, row + 2, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column + 1, row - 2, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column + 2, row + 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column + 2, row - 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column - 1, row + 2, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column - 1, row - 2, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column - 2, row + 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column - 2, row - 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);

    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;

    return bestValue;
}

float Bishop::bestMoveWithThisPieceScoreOrdered(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const {
    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position
    board.playerOnMove = oppositeSide(board.playerOnMove);

    static_vector<std::pair<float, std::pair<i8, i8>>, 13> possibleMoves;

    for (i8 i = 1; addMoveToList(board, column + i, row + i, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column + i, row - i, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column - i, row + i, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column - i, row - i, alpha, beta, possibleMoves); ++i);

    switch (occupancy())
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


    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();

    for (const auto& i : possibleMoves)
        tryPlacingPieceAt(board, i.second.first, i.second.second, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);


    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;

    return bestValue;
}

float Bishop::bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const {

    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();
    //if (depth <= 0)
      //  return 0;

    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position

    board.playerOnMove = oppositeSide(board.playerOnMove);

    for (i8 i = 1; tryPlacingPieceAt(board, column + i, row + i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column + i, row - i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column - i, row + i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column - i, row - i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);

    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;


    return bestValue;
}


float Rook::bestMoveWithThisPieceScoreOrdered(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const {
    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position
    board.playerOnMove = oppositeSide(board.playerOnMove);

    static_vector<std::pair<float, std::pair<i8, i8>>, 14> possibleMoves;

    for (i8 i = 1; addMoveToList(board, column + i, row, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column - i, row, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column, row + i, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column, row - i, alpha, beta, possibleMoves); ++i);

    switch (occupancy())
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


    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();

    for (const auto& i : possibleMoves)
        tryPlacingPieceAt(board, i.second.first, i.second.second, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);


    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;

    return bestValue;
}

float Rook::bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const {
    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();

    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position

    board.playerOnMove = oppositeSide(board.playerOnMove);

    bool castleLeftBackup;
    bool castleRightBackup;

    if (initialRow() == row)
    {
        if (column == 0)
        {
            bool& canICastleLeft = board.canCastleLeft[(occupancy() + 1) / 2];
            castleLeftBackup = canICastleLeft;
            canICastleLeft = false;
        }
        else if (column == 7)
        {
            bool& canICastleRight = board.canCastleRight[(occupancy() + 1) / 2];
            castleRightBackup = canICastleRight;
            canICastleRight = false;
        }
    }

    for (i8 i = 1; tryPlacingPieceAt(board, column, row + i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column, row - i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column + i, row, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column - i, row, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);

    if (initialRow() == row)
    {
        if (column == 0)
        {
            bool& canICastleLeft = board.canCastleLeft[(occupancy() + 1) / 2];
            canICastleLeft = castleLeftBackup;
        }
        else if (column == 7)
        {
            bool& canICastleRight = board.canCastleRight[(occupancy() + 1) / 2];
            canICastleRight = castleRightBackup;
        }
    }

    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;

    return bestValue;
}

float Queen::bestMoveWithThisPieceScoreOrdered(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const {
    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position
    board.playerOnMove = oppositeSide(board.playerOnMove);

    static_vector<std::pair<float, std::pair<i8, i8>>, 27> possibleMoves;
    for (i8 i = 1; addMoveToList(board, column + i, row + i, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column + i, row - i, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column - i, row + i, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column - i, row - i, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column + i, row, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column - i, row, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column, row + i, alpha, beta, possibleMoves); ++i);
    for (i8 i = 1; addMoveToList(board, column, row - i, alpha, beta, possibleMoves); ++i);

    switch (occupancy())
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


    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();

    for (const auto& i : possibleMoves)
        tryPlacingPieceAt(board, i.second.first, i.second.second, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);


    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;

    return bestValue;
}

float Queen::bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const {
    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();

    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position

    board.playerOnMove = oppositeSide(board.playerOnMove);


    for (i8 i = 1; tryPlacingPieceAt(board, column + i, row + i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column + i, row - i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column - i, row + i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column - i, row - i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);

    for (i8 i = 1; tryPlacingPieceAt(board, column, row + i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column, row - i, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column + i, row, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);
    for (i8 i = 1; tryPlacingPieceAt(board, column - i, row, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar); ++i);

    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;

    return bestValue;
}

float King::bestMoveWithThisPieceScore(Board& board, i8 column, i8 row, i8 depth, float& alpha, float& beta, float valueSoFar, bool doNoContinue) const {
    float bestValue = std::numeric_limits<float>::infinity() * (-1) * occupancy();

    board.pieceAt(column, row) = nullptr;
    valueSoFar -= priceAdjustmentPov(column, row) * occupancy();//We are leaving our current position

    board.playerOnMove = oppositeSide(board.playerOnMove);

    bool& canICastleLeft = board.canCastleLeft[(occupancy() + 1) / 2];
    bool& canICastleRight = board.canCastleLeft[(occupancy() + 1) / 2];

    bool castleLeftBackup = canICastleLeft;
    bool castleRightBackup = canICastleRight;

    canICastleLeft = false;
    canICastleRight = false;

    tryPlacingPieceAt(board, column + 1, row + 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column + 1, row, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column + 1, row - 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column - 1, row + 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column - 1, row, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column - 1, row - 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column, row + 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);
    tryPlacingPieceAt(board, column, row - 1, depth - 1, alpha, beta, bestValue, doNoContinue, valueSoFar);

    canICastleLeft = castleLeftBackup;
    canICastleRight = castleRightBackup;


#ifndef CASTLING_DISABLED
    if (canICastleLeft)//Neither has moved
    {
        AssertAssume(column == 4);//King has to be in initial position
        AssertAssume(row == 0 || row == 7);
        tryCastling<0, 3>(board, row, canICastleLeft, canICastleRight, bestValue, depth, alpha, beta, valueSoFar, doNoContinue);
    }
    if (canICastleRight)//Neither has moved
    {
        AssertAssume(column == 4);//King has to be in initial position
        AssertAssume(row == 0 || row == 7);
        tryCastling<7, 5>(board, row, canICastleLeft, canICastleRight, bestValue, depth, alpha, beta, valueSoFar, doNoContinue);
    }
#endif

    board.playerOnMove = oppositeSide(board.playerOnMove);
    board.pieceAt(column, row) = this;

    return bestValue;
}




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
void printLowerBound()
{
    auto os = std::osyncstream(out);
    os << "info " << "depth " << depthW + 1 << ' ';
    printScore(os, alphaOrBeta, depthW + 1, oppositeSide(onMoveW));
    os << ' ';
    os << "lowerbound";
    os << nl << std::flush;
}
void evaluateGameMove(GameMove& board, i8 depth)//, double alpha = -std::numeric_limits<float>::max(), double beta = std::numeric_limits<float>::max())
{
    //auto timeStart = std::chrono::high_resolution_clock::now();
    Board localBoard(board.researchedBoard);

    if (options.MultiPV > 1) [[unlikely]]//If we want to know multiple good moves, we cannot prune using a/B at root leve¨l
    {
        board.bestFoundValue = localBoard.bestMoveScore(depth, board.startingValue, -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
        //board.researchedTime = std::chrono::high_resolution_clock::now() - timeStart;
    }

    else
    {
        switch (localBoard.playerOnMove)
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
}


static std::vector<GameMove*> q;
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

        AssertAssume(tmp.researchedBoard.playerOnMove == onMoveW);

        evaluateGameMoveFromQ(localPos, depthW);
    }
}

void cutoffBadMoves(std::vector<GameMove>& boards)
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

std::chrono::duration<float, std::milli> findBestOnSameLevel(std::vector<GameMove>& boards, i8 depth)//, PlayerSide onMove)
{
    AssertAssume(!boards.empty());
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

    auto timeThisStarted = std::chrono::high_resolution_clock::now();
    if (depth > 0)
    {
        alphaOrBeta = std::numeric_limits<float>::max() * onMoveResearched;
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
    auto elapsedThis = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - timeThisStarted);

    if (qPos != boards.size())
    {
        std::cerr << "Not enough time to search all moves. Only managed to fully go through " << qPos << " out of " << boards.size() << std::endl;
    }

    boards.resize(qPos);

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
    

    auto elapsedTotal = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - timeGlobalStarted);


    float scoreCp = boards.front().bestFoundValue;
    //std::cerr << "ScoreCP: " << scoreCp << std::endl;

    //if (itsTimeToStop)
    //    return;


    for (size_t i = boards.size()-1; i != 0; --i)
        printMoveInfo(depth + 1, elapsedTotal.count(), boards[i], i + 1, options.MultiPV == 1, oppositeSide(onMoveW));

    printMoveInfo(depth + 1, elapsedTotal.count(), boards.front(), 1, false, oppositeSide(onMoveW));

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


std::vector<GameMove> generateMoves(Board& board, PlayerSide bestForWhichSide)//, i8 depth = 1
{
    static auto rd = std::random_device{};
    static auto rng = std::default_random_engine{ rd() };
    alphaOrBeta = std::numeric_limits<float>::max() * board.playerOnMove;
    const i8 depth = 1;
    itsTimeToStop = false;
    onMoveW = board.playerOnMove;
    depthW = 1;
    saveToVector = true;

    out << "info depth 1" << nl << std::flush;
    //transpositions.clear();
    board.bestMoveScore(1, 0, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
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
    //        pos.researchedBoard.bestMoveScore(depth - 1, pos.startingValue, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
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

std::pair<Board, float> findBestOnTopLevel(Board& board, i8 depth, PlayerSide onMove)
{
    auto tmp = generateMoves(board, onMove);//, onMove);
    findBestOnSameLevel(tmp, depth - 1);//, oppositeSide(onMove));
    const auto& res = tmp.front();
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
    board.movePiece(move[0], move[1], move[2], move[3]);

    //Castling
    if (move == "e1c1" && board.pieceAt(move[2], move[3]) == &kingWhite)
        board.movePiece('a', '1', 'd', '1');
    else if (move == "e1g1" && board.pieceAt(move[2], move[3]) == &kingWhite)
        board.movePiece('h', '1', 'f', '1');
    else if (move == "e8c8" && board.pieceAt(move[2], move[3]) == &kingBlack)
        board.movePiece('a', '8', 'd', '8');
    else if (move == "e8g8" && board.pieceAt(move[2], move[3]) == &kingBlack)
        board.movePiece('h', '8', 'f', '8');

    //Pawn promotion
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
        board.pieceAt(move[2], move[3]) = evolvedInto;
    }

    //En passant
    else if (move[1] == '5' && board.pieceAt(move[2], move[3]) == &pawnWhite && move[0] != move[2] && backup == nullptr)
    {
        board.pieceAt(move[2], move[3] - 1) = nullptr;
    }
    else if (move[1] == '4' && board.pieceAt(move[2], move[3]) == &pawnBlack && move[0] != move[2] && backup == nullptr)
    {
        board.pieceAt(move[2], move[3] + 1) = nullptr;
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


Board startingPosition()
{
    Board initial;
    initial.playerOnMove = PlayerSide::WHITE;
    initial.canCastleLeft.fill(true);
    initial.canCastleRight.fill(true);

    initial.pieceAt('a', '1') = &rookWhite;
    initial.pieceAt('b', '1') = &knightWhite;
    initial.pieceAt('c', '1') = &bishopWhite;
    initial.pieceAt('d', '1') = &queenWhite;
    initial.pieceAt('e', '1') = &kingWhite;
    initial.pieceAt('f', '1') = &bishopWhite;
    initial.pieceAt('g', '1') = &knightWhite;
    initial.pieceAt('h', '1') = &rookWhite;

    initial.pieceAt('a', '2') = &pawnWhite;
    initial.pieceAt('b', '2') = &pawnWhite;
    initial.pieceAt('c', '2') = &pawnWhite;
    initial.pieceAt('d', '2') = &pawnWhite;
    initial.pieceAt('e', '2') = &pawnWhite;
    initial.pieceAt('f', '2') = &pawnWhite;
    initial.pieceAt('g', '2') = &pawnWhite;
    initial.pieceAt('h', '2') = &pawnWhite;

    initial.pieceAt('a', '8') = &rookBlack;
    initial.pieceAt('b', '8') = &knightBlack;
    initial.pieceAt('c', '8') = &bishopBlack;
    initial.pieceAt('d', '8') = &queenBlack;
    initial.pieceAt('e', '8') = &kingBlack;
    initial.pieceAt('f', '8') = &bishopBlack;
    initial.pieceAt('g', '8') = &knightBlack;
    initial.pieceAt('h', '8') = &rookBlack;

    initial.pieceAt('a', '7') = &pawnBlack;
    initial.pieceAt('b', '7') = &pawnBlack;
    initial.pieceAt('c', '7') = &pawnBlack;
    initial.pieceAt('d', '7') = &pawnBlack;
    initial.pieceAt('e', '7') = &pawnBlack;
    initial.pieceAt('f', '7') = &pawnBlack;
    initial.pieceAt('g', '7') = &pawnBlack;
    initial.pieceAt('h', '7') = &pawnBlack;
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

GameMove findBestInNumberOfMoves(Board& board, i8 moves)
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

            std::array<double, 2> playerTime;
            std::array<double, 2> playerInc;
            //int64_t wtime = 0, btime = 0, winc = 0, binc = 0;
            double timeTargetMax = 0;
            double timeTargetOptimal = 0;
            i8 maxDepth = std::numeric_limits<i8>::max();

            while (true)
            {
                auto word = getWord(commandView);
                if (word.empty())
                    break;
                else if (word == "wtime")
                    playerTime[(PlayerSide::WHITE + 1) / 2] = atoll(getWord(commandView).data());
                else if (word == "btime")
                    playerTime[(PlayerSide::BLACK + 1) / 2] = atoll(getWord(commandView).data());
                else if (word == "winc")
                    playerInc[(PlayerSide::WHITE + 1) / 2] = atoll(getWord(commandView).data());
                else if (word == "binc")
                    playerInc[(PlayerSide::BLACK + 1) / 2] = atoll(getWord(commandView).data());
                else if (word == "movetime")
                {
                    timeTargetMax = atoll(getWord(commandView).data());
                    timeTargetOptimal = timeTargetMax;
                }
                else if (word == "infinite")
                {
                    timeTargetMax = std::numeric_limits<double>::infinity();
                    timeTargetOptimal = std::numeric_limits<double>::infinity();
                }
                else if (word == "depth")
                    maxDepth = atoll(getWord(commandView).data());
            }

            if (timeTargetMax == 0)//We have to calculate our own time
            {
                float gamePhase = (17.0f - board.countPiecesMin()) / 16.0f;

                const int64_t& myTime = playerTime[(board.playerOnMove + 1) / 2];// == PlayerSide::WHITE ? wtime : btime;
                const int64_t& myInc = playerTime[(board.playerOnMove + 1) / 2];// == PlayerSide::WHITE ? winc : binc;

                timeTargetOptimal = myInc + myTime * gamePhase / 6;
                timeTargetMax = myInc + myTime * gamePhase / 3;
            }


            std::cerr << "Targeting " << timeTargetOptimal << " ms." << std::endl;
            std::cerr << "Highest I can go is " << timeTargetMax << " ms." << std::endl;


            //dynamicPositionRanking = false;
            auto boardList = generateMoves(board,board.playerOnMove);
            //dynamicPositionRanking = true;

            GameMove bestPosFound;

            std::vector<std::pair<double, float>> previousResults;

            //std::cerr << "Depth: ";

            for (i8 i = 2; i <= maxDepth; i += 2) {
                out << "info " << "depth " << unsigned(i) << nl << std::flush;

                size_t availableMoveCount = boardList.size();
                std::chrono::duration<double, std::milli> elapsedThisDepth = findBestOnSameLevel(boardList, i);
                bool searchedThroughAllMoves = boardList.size() == availableMoveCount;


                bestPosFound = boardList.front();

                auto elapsedTotal = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - timeGlobalStarted).count();

                float scoreCp = bestPosFound.bestFoundValue;


                if (round(abs(scoreCp) / matePrice) > 0)
                    break;

                //dynamicPositionRanking = false;

                //res = bestPosFound;
                std::cerr << "Elapsed in this depth: " << elapsedThisDepth.count() << std::endl;;
                //std::cerr << i + 1 << ' ';


                //std::cerr << " is " << elapsed << std::endl;

                if (elapsedTotal >= timeTargetMax)//Emergency stop if we depleted time
                    break;
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

                    if (diff < 100)//The difference is too small, we probably wouldn't get much further info.
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
                promotion.pieceAt('h', '5') = &pawnBlack;
                promotion.pieceAt('d', '5') = &pawnBlack;
                promotion.pieceAt('f', '5') = &kingBlack;
                promotion.pieceAt('g', '1') = &bishopBlack;
                promotion.pieceAt('e', '2') = &kingWhite;
                promotion.pieceAt('b', '5') = &pawnWhite;
                promotion.pieceAt('a', '6') = &pawnWhite;
                promotion.playerOnMove = PlayerSide::WHITE;

                promotion.print();

                benchmark(8, promotion);
            } break;
            case (2):
            {
                Board testMatu;
                testMatu.pieceAt('h', '8') = &kingBlack;
                testMatu.pieceAt('a', '1') = &kingWhite;
                testMatu.pieceAt('g', '1') = &rookWhite;
                testMatu.pieceAt('a', '7') = &rookWhite;
                testMatu.pieceAt('b', '1') = &queenWhite;
                testMatu.pieceAt('c', '7') = &pawnWhite;
                testMatu.playerOnMove = PlayerSide::WHITE;
                testMatu.print();

                benchmark(8, testMatu);
            } break;
            case (3):
            {
                Board testMatu;
                testMatu.pieceAt('h', '8') = &kingBlack;
                testMatu.pieceAt('h', '7') = &pawnWhite;
                testMatu.pieceAt('g', '6') = &pawnWhite;
                testMatu.pieceAt('h', '6') = &kingWhite;
                testMatu.pieceAt('h', '5') = &pawnWhite;
                testMatu.pieceAt('g', '5') = &pawnWhite;

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
                Board test = startingPosition();

                test.pieceAt('b', '1') = nullptr;
                test.pieceAt('c', '1') = nullptr;
                test.pieceAt('d', '1') = nullptr;
                test.pieceAt('f', '1') = nullptr;
                test.pieceAt('g', '1') = nullptr;

                test.playerOnMove = PlayerSide::WHITE;
                test.print();

                benchmark(8, test);
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