import asyncio
import websockets
import json
from argparse import ArgumentParser, FileType
from confluent_kafka import Producer
from configparser import ConfigParser

msg_count = 0

def delivery_callback(err, msg):
    if err:
        print('ERROR: Message failed delivery: {}'.format(err))
    else:
        print("Produced event to topic {topic}: key = {key:12} value = {value:12}".format(
            topic=msg.topic(), key=msg.key().decode('utf-8'), value=msg.value().decode('utf-8')))

async def consumer(message, producer):
    global msg_count
    coin_data : dict[str, str] = json.loads(message)
    for (coin, value) in coin_data.items():
        producer.produce(coin, key=str(msg_count), value=bytes(value, encoding= 'utf-8'), callback=delivery_callback)
        msg_count+=1
        producer.poll(0)

async def hello(uri, config):
    async for websocket in websockets.connect(uri):
        try:
            producer = Producer(config)
            async for message in websocket:
                await consumer(message, producer)
        except Exception as e:
            print(e)
            continue


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument('-c', '--config', type=FileType('r'))
    parser.add_argument('-t', '--topics', type=FileType('r'), required=True)
    args = parser.parse_args()

    config_parser = ConfigParser()
    config_parser.read_file(args.config)
    config = dict(config_parser['default'])

    coins : dict = json.load(args.topics)
    if "Coins" not in coins:
        raise Exception("Coins not found in json file.")

    coins_to_monitor = ','.join(coins["Coins"])

    asyncio.run(hello(f'wss://ws.coincap.io/prices?assets={coins_to_monitor}', config))

