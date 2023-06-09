# CSS30-EventDriverArchitecture

Trabalho desenvolvido para a Matéria de Sistemas Distribuídos. O trabalho consiste em um produtor (presente no arquivo `criptocoin_producer.py`) e um consumidor (presente no arquivo `Criptocoin_Consumer_UI/Criptocoin_Consumer_UI.cpp`). Ambos usam o broker do Kafka para a trocar de mensagens.

## Produtor

O produtor utiliza um websocket fornnecido pelo `https://coincap.io` que envia um evento a cada variação da cotação de uma moeda escolhida. Essas moedas são definidas no arquivo `Criptocoins.json`. Os nomes das moedas passíveis de acompanhamento podem ser vistas através desse [link](api.coincap.io/v2/assets
)
A cada evento eviando do websocket do coincap.io a função `consumer()` que verifica quantas moedas tiveram seu valor alterado e envia um evento para o kafka para cada uma delas.

Cada moeda definida no arquivo `Criptocoins.json` representa um tópico que o consumidor podera assinar interesse.

Como executar:
```bash
$ python criptocoin_producer.py -t Criptocoins.json -c kafka/config.ini
```
obs:Foi usado python 3 no desenvilvimento. Não garantimos o correto funcionamento em outras versões.
## Consumidor
O consumidor possui uma interface que facilita tanto a escolha de quais moedas acompanhar, quanto possibilita a visão em forma de gráfico das variações das moedas.
Ao selecionar uma moeda no checkbox para acompanhar, a função `ShowRealTimeData()` faz a função de adicionar a moeda ao vetor `consumers`, que é usado para montar o gráfico.
O construtor da classe `KafkaConsumer` cria um ponteiro que escuta os tópicos definidos.
Para executar:
Entre na pasta Criptocoin_Consumer_UI e digite os comandos abaixo para instalar as dependencias e compilar o projeto.
```bash
$ git clone https://github.com/ocornut/imgui.git
$ git clone https://github.com/epezent/implot.git
$ sudo apt install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config librdkafka-dev libglew-dev libglfw3-dev
$ mkdir build && cd build
$ cmake ..
$ make
$ ./css30
```

## Kafka
O kafka pode ser executado da melhor forma para o usuário. Pode ser rodando local ou por docker. O importante é apontar nos arquivos `kafka/config.ini` e no arquivo `Criptocoin_Consumer_UI/Criptocoin_Consumer_UI.cpp` para o host e porta em que o seu kafka está rodando.


## Diagrama simples do funcionamento

![](./diagrama_aplicacao.png)