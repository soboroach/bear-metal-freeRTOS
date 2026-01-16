#include <inttypes.h>
#include <stdbool.h>

#define BIT(x) (1UL << (x))
#define PIN(bank, num) ((((bank) - 'A') << 8) | (num))
#define PINNO(pin) (pin & 255)
#define PINBANK(pin) (pin >> 8)

// --------------------------------------------------------------------------
// 레지스터 정의
// --------------------------------------------------------------------------
struct gpio {
  volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2];
};
#define GPIO(bank) ((struct gpio *)(0x40020000 + 0x400 * (bank)))

struct rcc {
  volatile uint32_t CR, PLLCFGR, CFGR, CIR, AHB1RSTR, AHB2RSTR, AHB3RSTR,
      RESERVED0, APB1RSTR, APB2RSTR, RESERVED1[2], AHB1ENR, AHB2ENR, AHB3ENR,
      RESERVED2, APB1ENR, APB2ENR, RESERVED3[2], AHB1LPENR, AHB2LPENR,
      AHB3LPENR, RESERVED4, APB1LPENR, APB2LPENR, RESERVED5[2], BDCR, CSR,
      RESERVED6[2], SSCGR, PLLI2SCFGR;
};
#define RCC ((struct rcc *)0x40023800)

enum { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, GPIO_MODE_AF, GPIO_MODE_ANALOG };

// --------------------------------------------------------------------------
// 함수 정의
// --------------------------------------------------------------------------

static inline void gpio_write(uint16_t pin, bool val) {
  struct gpio *gpio = GPIO(PINBANK(pin));
  // BSRR 레지스터를 통해 Atomic하게 비트 설정
  gpio->BSRR = (1U << PINNO(pin)) << (val ? 0 : 16);
}

static inline void gpio_set_mode(uint16_t pin, uint8_t mode) {
  struct gpio *gpio = GPIO(PINBANK(pin));
  int n = PINNO(pin);
  gpio->MODER &= ~(3U << (n * 2));
  gpio->MODER |= (mode & 3) << (n * 2);
}

// --------------------------------------------------------------------------
// 메인 함수
// --------------------------------------------------------------------------
int main(void) {
  uint16_t led = PIN('A', 5); // PA5 = Green LED

  // 1. 클럭 공급
  RCC->AHB1ENR |= BIT(PINBANK(led));

  // [중요] 클럭을 켠 직후 하드웨어가 준비될 시간을 아주 조금 줘야 합니다.
  // 단순히 레지스터를 한 번 읽어주는 것으로 충분합니다.
  volatile uint32_t dummy = RCC->AHB1ENR;
  (void)dummy;

  // 2. 모드 설정 (Output)
  gpio_set_mode(led, GPIO_MODE_OUTPUT);

  // 3. 무한 루프
  for (;;) {
    // 켜기
    gpio_write(led, true);

    // [수정] 함수 호출 없이 쌩으로 지연 (최적화 방지 volatile)
    // 숫자를 2,000,000 (200만) 정도로 크게 잡으세요.
    for (volatile uint32_t i = 0; i < 2000000; i++) {
      __asm("nop");
    }

    // 끄기
    gpio_write(led, false);

    // [수정] 끄고 나서도 똑같이 기다려야 눈에 보입니다.
    for (volatile uint32_t i = 0; i < 2000000; i++) {
      __asm("nop");
    }
  }
  return 0;
}

// --------------------------------------------------------------------------
// 스타트업 코드 (수정됨)
// --------------------------------------------------------------------------

// [수정] 'naked' 속성을 제거했습니다.
// naked 함수 안에서 로컬 변수(dst, src 등)를 쓰면 스택이 꼬여서
// 함수가 끝난 뒤나 내부 호출 시 뻗을(HardFault) 위험이 있습니다.
// noreturn만 남겨두세요.
__attribute__((noreturn)) void _reset(void) {
  extern long _sbss, _ebss, _sdata, _edata, _sidata;

  for (long *dst = &_sbss; dst < &_ebss; dst++)
    *dst = 0;

  for (long *dst = &_sdata, *src = &_sidata; dst < &_edata;)
    *dst++ = *src++;

  main();

  for (;;)
    (void)0;
}

extern void _estack(void);

// 벡터 테이블
__attribute__((section(".vectors"))) void (*const tab[16 + 97])(void) = {
    _estack, _reset};