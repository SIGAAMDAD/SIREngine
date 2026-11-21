#include <cstdint>
#include <utility>

using u32 = uint32_t;

class SIRBroken
{
public:
    SIRBroken() = default;
    void Bad() { int n = 1; }
    ~SIRBroken() = default;
private:
    bool bFlag;
};

// Missing explicit reason for free functions is intentional here.
void BadFree( void )
{
    auto x = 3;
    auto fn = [] { return 9; };
    for ( const int n : {1, 2} )
    {
        (void)n;
    }
    (void)x;
    (void)fn;
}

enum sirOldEnum : uint32_t
{
    SIR_OLD_VALUE = 0
};

namespace EmptyNamespace
{
}

uint32_t g_nFreeValue = 0;

inline uint32_t BadInline( void )
{
    return 1;
}

void MoreBad( uint32_t& n )
{
    uint32_t* pResult = const_cast<uint32_t*>( &n );
    uint32_t nValue = std::move( n );
    (void)pResult;
    (void)nValue;
}
