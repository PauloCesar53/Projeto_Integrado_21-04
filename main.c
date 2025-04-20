
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"//biblioteca para funções  pwm
#include "ws2812.pio.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/adc.h"

//definições da i2c
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define IS_RGBW false
#define NUM_PIXELS 25
#define WS2812_PIN 7
#define Frames 5
#define LED_PIN_B 12//LED que irá simular uma bonba de agua para irrigação 
#define Botao_A 5 // gpio do botão A na BitDogLab
#define Botao_B 6 // gpio do botão B na BitDogLab
#define JOY_Y 26//eixo  y segundo diagrama BitDogLab (Sensor de Temperatura Simulado)
#define JOY_X 27//eixo  x segundo diagrama BitDogLab (Sensor de umidade do Solo resistivo Simulado)
#define buzzer 21// pino do Buzzer na BitDogLab
#define  Botao_JOY 22
//definição de variáveis que guardarão valores auxiliares para o botão 
bool aux_Bot_B= true;//aulixar para escolher o que mostrar na matriz de LEDs
uint8_t aux_Bot_A= 1;//aulixar para escolher o que mostrar no Display 
bool aux_Bot_JOY= true;//aulixar para destivar alarme sonoro 


// Variável global para armazenar a cor (Entre 0 e 255 para intensidade)
uint8_t led_r = 0;  // Intensidade do vermelho
uint8_t led_g = 0; // Intensidade do verde
uint8_t led_b = 5; // Intensidade do azul (inicia mostrando umidade do solo na matriz)

//variáveis globais 
static volatile int aux = 1; // posição do numero impresso na matriz, inicialmente imprime numero 5
static volatile uint32_t last_time_A = 0; // Armazena o tempo do último evento para Bot A(em microssegundos)
static volatile uint32_t last_time_B = 0; // Armazena o tempo do último evento para Bot B(em microssegundos)
static volatile uint32_t last_time_JOY = 0; // Armazena o tempo do último evento para Bot JOY(em microssegundos)
static volatile uint32_t last_time_T = 0;// Armazena o tempo do último evento para Bot Calular intervalor para medir T
int His_T[30]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};//veotor com histórico de Temperaturas 

bool led_buffer[NUM_PIXELS];// Variável (protótipo)
bool buffer_Numeros[Frames][NUM_PIXELS];// // Variável (protótipo) 
    
//protótipo funções que ligam leds da matriz 5x5
void atualiza_buffer(bool buffer[], bool b[][NUM_PIXELS], int c); ///protótipo função que atualiza buffer
static inline void put_pixel(uint32_t pixel_grb);
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void set_one_led(uint8_t r, uint8_t g, uint8_t b);//liga os LEDs escolhidos 

void gpio_irq_handler(uint gpio, uint32_t events);// protótipo função interrupção para os botões A e B  e JOY
void Sensor_Matiz_5X5(uint joy_x, uint joy_y);//// protótipo função que escolhe qual sensor será representado na matriz 
void Imprime_5X5(uint rate);// protótipo função que mostra percentual do  sensor  na matriz 5x5
void Monitoramento(uint joy_x, uint joy_y, ssd1306_t c, bool b);// protótipo função que mostra monitoramento dos sensores no Display
void Irrigacao(uint joy_x);// Protótipo de função para acionar bomba d'água 
void pwm_init_gpio(uint gpio, uint wrap);//protótipo de função para configurar pwm
void alerta_umidade(uint joy_x);//protótipo de função gerar alerta sonoro de umidade 
void history_T(uint16_t joy_y);// protótipo de função armazena maior Temperatura do intervalo no Histórico 
void His_T_Dislay(ssd1306_t c, bool b);// protótipo de função para mostrar histórico de Temperatura no Display

int main()
{
    //Inicializando I2C . Usando 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);// Definindo a função do pino GPIO para I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);// Definindo a função do pino GPIO para I2C
    gpio_pull_up(I2C_SDA);// definindo como resistência de pull up
    gpio_pull_up(I2C_SCL);// definindo como resistência de pull up
    ssd1306_t ssd;// Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);// Inicializa o display
    ssd1306_config(&ssd);// Configura o display
    ssd1306_send_data(&ssd);// Envia os dados para o display
    bool cor = true;
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
  
    //configuração PIO
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    
    stdio_init_all(); // Inicializa comunicação padrão
    adc_init();//inicializando adc
    adc_gpio_init(JOY_Y);//inicializando pino direção y 
    adc_gpio_init(JOY_X);//inicializando pino direção x

    //configurando PWM
    uint pwm_wrap = 8000;// definindo valor de wrap referente a 12 bits do ADC
    pwm_init_gpio(buzzer, pwm_wrap); 

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
    
    // configuração led RGB azul
    gpio_init(LED_PIN_B);              // inicializa pino do led azul
    gpio_set_dir(LED_PIN_B, GPIO_OUT); // configrua como saída
    gpio_put(LED_PIN_B, 0);            // led inicia apagado
    
    // configuração botão A
    gpio_init(Botao_A);             // inicializa pino do botão A
    gpio_set_dir(Botao_A, GPIO_IN); // configura como entrada
    gpio_pull_up(Botao_A);          // Habilita o pull-up interno
    // configuração botão B
    gpio_init(Botao_B);             // inicializa pino do botão B
    gpio_set_dir(Botao_B, GPIO_IN); // configura como entrada
    gpio_pull_up(Botao_B);          // Habilita o pull-up interno
    // configuração botão joy
    gpio_init(Botao_JOY);             // inicializa pino do botão JOY
    gpio_set_dir(Botao_JOY, GPIO_IN); // configura como entrada
    gpio_pull_up(Botao_JOY);          // Habilita o pull-up interno

    // configurando a interrupção com botão na descida para o botão A
    gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // configurando a interrupção com botão na descida para o botão B
    gpio_set_irq_enabled_with_callback(Botao_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // configurando a interrupção com botão na descida para o botão JOY
    gpio_set_irq_enabled_with_callback(Botao_JOY, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    bool collor = true;
    while (1)
    {
        
        adc_select_input(0);//canal adc JOY para eixo y
        uint16_t JOY_Y_value = (adc_read()/4095.0)*50; // Lê o valor do eixo y, de 0 a 4095 e calcula temperatura (0 a 50°C)
        adc_select_input(1);//canal adc JOY para eixo x
        uint16_t JOY_X_value = (adc_read()/4095.0)*100;// Lê o valor do eixo x, de 0 a 4095 e  calcula % umidade
        history_T(JOY_Y_value);//calcula e armazena historico de temperatura em 30 intervalos 
        Sensor_Matiz_5X5(JOY_X_value, JOY_Y_value);//escolhe qual sensor mostrar na matriz de LEDs
        alerta_umidade(JOY_X_value);//alerta sonoro se umidade cair a baixo de 15%
        Irrigacao(JOY_X_value);//Liga LED azul simulando acionamento de bomba D'água
        if(aux_Bot_A==1){//escolhe o que mostrar no display 
            Monitoramento(JOY_X_value, JOY_Y_value, ssd, collor);//mostra monitoramento em tempo real 
        }else{
            His_T_Dislay(ssd, collor);//mostra histórico de Temperatura 
        }
    }
    return 0;
}

bool led_buffer[NUM_PIXELS] = { //Buffer para armazenar quais LEDs estão ligados matriz 5x5
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0};

bool buffer_Numeros[Frames][NUM_PIXELS] =//Frames que formam nível do sensor mostrado na matriz
    {
      //{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24} referência para posição na BitDogLab matriz 5x5
        {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // para o número zero (até 20% de umidade)
        {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // para o número 1 (entre 20% e 40% de umidade)
        {0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // para o número 2 (entre 40% e 60% de umidade)
        {0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // para o número 3 (entre 60% e 80% de umidade)
        {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // para o número 4 (acima de 80% de umidade)
};

// função que atualiza o buffer de acordo com frames indicativos dos sensores
void atualiza_buffer(bool buffer[],bool b[][NUM_PIXELS], int c)
{
    for (int i = 0; i < NUM_PIXELS; i++)
    {
        buffer[i] = b[c][i];
    }
}
static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}
void set_one_led(uint8_t r, uint8_t g, uint8_t b)
{
    // Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(r, g, b);

    // Define todos os LEDs com a cor especificada
    for (int i = 0; i < NUM_PIXELS; i++)
    {
        if (led_buffer[i])
        {
            put_pixel(color); // Liga o LED com um no buffer
        }
        else
        {
            put_pixel(0); // Desliga os LEDs com zero no buffer
        }
    }
}
// função interrupção para os botões A e B  e JOY com condição para deboucing
void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t current_time = to_us_since_boot(get_absolute_time());//// Obtém o tempo atual em microssegundos
    if (gpio_get(Botao_A) == 0 &&  (current_time - last_time_A) > 200000)//200ms de boucing adiconado como condição 
    { // se botão A for pressionado 
        last_time_A = current_time; // Atualiza o tempo do último evento
        aux_Bot_A+=1;
        if(aux_Bot_A==5){
            aux_Bot_A=1;
        }
    }
    if (gpio_get(Botao_B) == 0  && (current_time - last_time_B) > 200000)//200ms de boucing adiconado como condição 
    { // se botão B for pressionado 
        last_time_B = current_time; // Atualiza o tempo do último evento
        aux_Bot_B=!aux_Bot_B;//altera o sensor para demostrar na matriz de LEDs
    }
    if (gpio_get(Botao_JOY) == 0  && (current_time - last_time_JOY) > 200000)//200ms de boucing adiconado como condição 
    { 
        last_time_JOY = current_time; // Atualiza o tempo do último evento
        aux_Bot_JOY=!aux_Bot_JOY;//acionar ou não alarme sonoro 
    }

}
//função que coloca percentual do sensor na matriz de LEDs
void Imprime_5X5(uint rate){//
    if(rate <=20){
        atualiza_buffer(led_buffer, buffer_Numeros, 0); 
        set_one_led(led_r, led_g, led_b);
    }else if(rate >20 && rate<=40){
        atualiza_buffer(led_buffer, buffer_Numeros, 1); 
        set_one_led(led_r, led_g, led_b);
    }else if(rate >40 && rate<=60){
        atualiza_buffer(led_buffer, buffer_Numeros, 2); 
        set_one_led(led_r, led_g, led_b); 
    }else if(rate >60 && rate<80){
        atualiza_buffer(led_buffer, buffer_Numeros, 3); 
        set_one_led(led_r, led_g, led_b); 
    }else if(rate >=80){
        atualiza_buffer(led_buffer, buffer_Numeros, 4); 
        set_one_led(led_r, led_g, led_b);
    }
}
//Função que mostra monitoramente de temperatura e umidade no display em tempo real 
void Monitoramento(uint joy_x, uint joy_y, ssd1306_t c, bool b){
    char str_x[5];  // Buffer para armazenar a string
    char str_y[5];  // Buffer para armazenar a string
    sprintf(str_x, "%d", joy_x);                     // Converte o inteiro em string
    sprintf(str_y, "%d", joy_y);                     // Converte o inteiro em string
    ssd1306_fill(&c, !b);                            // Limpa o display
    ssd1306_rect(&c, 3, 3, 122, 60, b, !b);          // Desenha um retângulo
    ssd1306_line(&c, 3, 14, 123, 14, b);             // Desenha uma linha
    ssd1306_draw_string(&c, "MONITORAMENTO", 12, 5); // Desenha uma string
    ssd1306_draw_string(&c, "UMIDADE", 8, 16);
    if(aux_Bot_JOY){
        ssd1306_draw_string(&c, "  ALERTA LIG", 7, 53);
    }else{
        ssd1306_draw_string(&c, "  ALERTA DES", 7, 53);
    }
    ssd1306_draw_string(&c, " T yC", 77, 16); // Desenha uma string (y equivale a ° na font.h)
    ssd1306_line(&c, 75, 15, 75, 49, b);      // Desenha uma linha vertical
    ssd1306_draw_string(&c, str_x, 21, 35);   // Desenha uma string
    ssd1306_draw_string(&c, "z", 37, 35);     // z equivale a % na font.h
    ssd1306_draw_string(&c, str_y, 91, 35);   // Desenha uma string
    ssd1306_line(&c, 3, 50, 123, 50, b);             // Desenha uma linha
    ssd1306_send_data(&c);                    // Atualiza o display
}
// Função para simular acionamento de bomba d'água com LED azul
void Irrigacao(uint joy_x){
    if(joy_x<=20){//aciona bomda d'agua para irrigação se umidade chegar a 20%
        gpio_put(LED_PIN_B, 1);
    }else if(joy_x>=60){//desliga bomba d'agua para irrigação se umidade chegar a 60%
        gpio_put(LED_PIN_B, 0);
    }
}
//função que escolhe qual sensor será representado na matriz de LEDs 
void Sensor_Matiz_5X5(uint joy_x, uint joy_y){
    if(aux_Bot_B){//mostra percentual de umidade na matriz de LEDs 
        led_b=5;
        led_r=0;
        Imprime_5X5(joy_x);//imprime umidade do solo Matriz de LEDs
    }else{//mostra percentual de temperatura na matriz de LEDs
        led_b=0;
        led_r=5;
        Imprime_5X5(2*joy_y);//imprime umidade do solo Matriz de LEDs
        }
}
//função para configuração do PWM
void pwm_init_gpio(uint gpio, uint wrap) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_clkdiv(slice_num, 125.0);//divisor de clock 
    pwm_set_enabled(slice_num, true);  
}
//função que gera alerta sonoro de umidade 
void alerta_umidade(uint joy_x){
    if(aux_Bot_JOY==0){
        pwm_set_gpio_level(buzzer, 0);//10% de Duty cycle
    }else if(joy_x<15){
        pwm_set_gpio_level(buzzer, 400);//10% de Duty cycle
    }else{
        pwm_set_gpio_level(buzzer, 0);//10% de Duty cycle
    }
}
//função que irá calcular o histórico de temperaturas  e armazenar em vetor com 30 intervalos(registra a maior temperatura do intervalo)
void history_T(uint16_t joy_y){   
    if(joy_y>His_T[29]){//para pegar maior temperatura do intervalo 
        His_T[29]=joy_y;
    }
    uint64_t current_time_T = (to_us_since_boot(get_absolute_time()))/1000000;//obtem tempo em segundos 
    if((current_time_T - last_time_T) > 5){//considera intervalos de 5 segungos para calculo de Temperatura
    //para dias substituir 5 por 86400 segundos 
        last_time_T = current_time_T; // Atualiza o tempo do último evento
        for(int i=0; i<29; i++){//modica posição no vetor de histórico de temperatura a cada intervalo 
            His_T[i]=His_T[i+1];
        }
        His_T[29]=joy_y;
    }
}
//Função para imprimir histórico  de Temperaturas no Display 
void His_T_Dislay(ssd1306_t c, bool b){
    char str_x[5];  // Buffer para armazenar a string
    int aux_a=31, aux_b=16;
    ssd1306_fill(&c, !b);                            // Limpa o display
    ssd1306_rect(&c, 3, 3, 122, 60, b, !b);          // Desenha um retângulo
    ssd1306_line(&c, 3, 14, 123, 14, b);             // Desenha uma linha
    ssd1306_draw_string(&c,"HISTORICO TyC", 6, 5); // Desenha uma string
    if(aux_Bot_A==2){//imprime da temperatura 1 até temperatura 10
        ssd1306_draw_string(&c, "T1      T6", 4, 16);  // Desenha uma string
        ssd1306_draw_string(&c, "T2      T7", 4, 25);  // Desenha uma string
        ssd1306_draw_string(&c, "T3      T8", 4, 34);  // Desenha uma string
        ssd1306_draw_string(&c, "T4      T9", 4, 43);  // Desenha uma string
        ssd1306_draw_string(&c, "T5      T10", 4, 52); // Desenha uma string
        for (int i = 0; i < 10; i++)
        {
            if(His_T[i]==-1){//para iprimir espaço vazio onde ainda não tem dados( onde Temperatura é -1 iniialmente)
                sprintf(str_x, " ");
            }else{
                sprintf(str_x, "%d", His_T[i]);
            }
            ssd1306_draw_string(&c, str_x, aux_a, aux_b); // Desenha uma string
            aux_b += 9;
            if (aux_b > 52)
            {
                aux_b = 16;
                aux_a += 69;
            }
        }
    }else if(aux_Bot_A==3){//imprime da temperatura 11 até temperatura 20
        aux_a=35;
        ssd1306_draw_string(&c, "T11     T16", 4, 16);  // Desenha uma string
        ssd1306_draw_string(&c, "T12     T17", 4, 25);  // Desenha uma string
        ssd1306_draw_string(&c, "T13     T18", 4, 34);  // Desenha uma string
        ssd1306_draw_string(&c, "T14     T19", 4, 43);  // Desenha uma string
        ssd1306_draw_string(&c, "T15     T20", 4, 52); // Desenha uma string
        for (int i = 10; i < 20; i++)
        {
            if(His_T[i]==-1){
                sprintf(str_x, " ");
            }else{
                sprintf(str_x, "%d", His_T[i]);
            }
            ssd1306_draw_string(&c, str_x, aux_a, aux_b); // Desenha uma string
            aux_b += 9;
            if (aux_b > 52)
            {
                aux_b = 16;
                aux_a += 65;
            }
        }
    }else if(aux_Bot_A==4){//imprime da temperatura 21 até temperatura 30
        aux_a=35;
        ssd1306_draw_string(&c, "T21     T26", 4, 16);  // Desenha uma string
        ssd1306_draw_string(&c, "T22     T27", 4, 25);  // Desenha uma string
        ssd1306_draw_string(&c, "T23     T28", 4, 34);  // Desenha uma string
        ssd1306_draw_string(&c, "T24     T29", 4, 43);  // Desenha uma string
        ssd1306_draw_string(&c, "T25     T30", 4, 52); // Desenha uma string
        for (int i = 20; i < 30; i++)
        {
            if(His_T[i]==-1){
                sprintf(str_x, " ");
            }else{
                sprintf(str_x, "%d", His_T[i]);
            }
            ssd1306_draw_string(&c, str_x, aux_a, aux_b); // Desenha uma string
            aux_b += 9;
            if (aux_b > 52)
            {
                aux_b = 16;
                aux_a += 65;
            }
        }
    }
    ssd1306_line(&c, 64, 15, 64, 61, b);      // Desenha uma linha vertical
    ssd1306_line(&c, 3, 23, 123, 23, b);             // Desenha uma linha
    ssd1306_line(&c, 3, 32, 123, 32, b);             // Desenha uma linha
    ssd1306_line(&c, 3, 41, 123, 41, b);             // Desenha uma linha
    ssd1306_line(&c, 3, 50, 123, 50, b);             // Desenha uma linha
    ssd1306_send_data(&c);                    // Atualiza o display
}