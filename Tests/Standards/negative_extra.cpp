#include <cstdint>

extern "C" void* malloc(uint64_t nSize);

// Intentionally invalid SIREngine policy fixture. Never build into the engine.
typedef uint32_t sirBadAlias_t;
using sirOtherAlias_t = uint32_t;
using namespace std;

enum class sirBadEnum : unsigned char
{
    SIR_BAD_ENUM = 0
};

struct sirBadStruct_t
{
    int nPrimitive;
    double fPrecision;
    bool bStored;
    uint32_t nValue;

    void TooManyStatements( void )
    {
        nValue = 0;
        nValue = 1;
        nValue = 2;
        nValue = 3;
    }
};

class SIRBadOrder
{
private:
    uint32_t m_nValue{};
public:
    explicit SIRBadOrder( void ) noexcept;
    explicit SIRBadOrder( const SIRBadOrder& ) = delete;
    explicit SIRBadOrder( SIRBadOrder&& ) = delete;
    ~SIRBadOrder( void ) noexcept;
    SIRBadOrder& operator=( const SIRBadOrder& ) = delete;
    SIRBadOrder& operator=( SIRBadOrder&& ) = delete;
protected:
};

SIRBadOrder::SIRBadOrder( void ) noexcept = default;
SIRBadOrder::~SIRBadOrder( void ) noexcept = default;

inline uint32_t g_nInlineValue = 0;

// Multiple banned statements in one body are intentional.
void MoreInvalidMethods( uint32_t nA, uint32_t nB, uint32_t nC, uint32_t nD, uint32_t nE )
{
    if ( nA && nB && nC && nD )
    {
        try
        {
            throw 1;
        }
        catch ( ... )
        {
            ::malloc( 2 );
        }
    }
}

inline uint32_t BadExplicitInline( void )
{
    return 7;
}

auto BadAutoReturn( void ) -> uint32_t
{
    return 6;
}

int BadPrimitiveReturn( void )
{
    return 5;
}
