#include <cstdint>

enum class sirResourceState : uint32_t
{
	SIR_RESOURCE_READY = 1
};

struct sirResourceDesc_t
{
	uint32_t nValue{};
	bool bEnabled : 1;

	[[nodiscard]] constexpr bool IsValid( void ) const noexcept
	{
		return nValue != 0;
	}
};

class SIRResource final
{
public:
	explicit SIRResource( void ) noexcept;
	explicit SIRResource( const SIRResource& ) = delete;
	explicit SIRResource( SIRResource&& ) noexcept = default;
	~SIRResource() noexcept;

	SIRResource& operator=( const SIRResource& ) = delete;
	SIRResource& operator=( SIRResource&& ) noexcept = default;

	uint32_t GetValue( void ) const noexcept;

private:
	uint32_t m_nValue{};
};

SIRResource::SIRResource( void ) noexcept = default;
SIRResource::~SIRResource() noexcept = default;

uint32_t SIRResource::GetValue( void ) const noexcept
{
	return m_nValue;
}
