# Projeto_Integrado_21-04
Repositório criado para versionamento do projeto da residência em software embarcado 


## Descrição do Funcionamento do programa 
O eixo X do joystick representa o nível de água em um reservatório, já o eixo Y do  joystick representa um sensor de temperatura, com os valores medidos sendo representados no Display em tempo real e no serial monitor. Quando o nível do reservatório chega cai para 20% ocorre o acionamento do LED RGB azul que simula uma bomba d’água, ficando acionado até que o nível do reservatório fique em 95%. São mostrados na matriz de LEDs uma representação do percentual dos sensores (inicialmente representando o nível do reservatório), podendo ser alterado o sensor com o acionamento do botão b da BitDogLab. O posicionamento dos joysticks é representado no Display por um retângulo de 8x8 pixels em tempo real. 
## Compilação e Execução

1. Certifique-se de que o SDK do Raspberry Pi Pico está configurado no seu ambiente.
2. Compile o programa utilizando a extensão **Raspberry Pi Pico Project** no VS Code:
   - Abra o projeto no VS Code, na pasta PROJETO_INTEGRADO_21-O4 tem os arquivos necessários para importar 
   o projeto com a extensão **Raspberry Pi Pico Project**.
   - Vá até a extensão do **Raspberry pi pico project** e após importar (escolher sdk 2.1.0) os projetos  clique em **Compile Project**.
3. Coloque a placa em modo BOOTSEL e copie o arquivo `Projeto.uf2`  que está na pasta build, para a BitDogLab conectado via USB.


**OBS1: Devem importar os projetos para gerar a pasta build, pois a mesma não foi inserida no repositório**
**OBS2: Em algumas versões da BitDogLab o posicionamento dos eixos X e Y podem estar invertidos**
**OBS3: O projeto foi produzido com base na versão do sdk 2.1.0, considerar esse fato quando for importar o projeto**


## Link com demonstração no youtube

Demonstração do funcionamento do projeto na BitDogLab: (https://youtu.be/KJc8__iscYs?si=hqLt6rvgnCMatCxF)


## Colaboradores
- [PauloCesar53 - Paulo César de Jesus Di Lauro ] (https://github.com/PauloCesar53)
