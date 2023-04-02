from confluent_kafka import Producer
from configparser import ConfigParser
from argparse import ArgumentParser, FileType
import requests
import threading
import time
import signal
from time import sleep
import asyncio
import websockets

def delivery_callback(err, msg):
    if err:
        print('ERROR: Message failed delivery: {}'.format(err))
    else:
        print("Produced event to topic {topic}: key = {key:12} value = {value:12}".format(
            topic=msg.topic(), key=msg.key().decode('utf-8'), value=msg.value().decode('utf-8')))

def busy_wait():
    while True:
        sleep(1)

def signal_handler(signum, frame):
    res = input("Ctrl-c was pressed. Do you really want to exit? y/n ")
    if res == 'y':
        exit(1)

def bitcoin(config):
    producer = Producer(config)
    while True:
        response = requests.get('https://www.mercadobitcoin.net/api/BTC/ticker/')
        data = response.json()
        producer.produce('BITCOIN', bytes(data["ticker"]["buy"], encoding= 'utf-8'), callback=delivery_callback)
        time.sleep(0.5)

def dogecoin(config):
    producer = Producer(config)
    while True:
        response = requests.get('https://www.mercadobitcoin.net/api/DOGE/ticker/')
        data = response.json()
        producer.produce('DOGECOIN', bytes(data["ticker"]["buy"], encoding= 'utf-8'), callback=delivery_callback)
        time.sleep(0.5)

parser = ArgumentParser()
parser.add_argument('-c','--config_file', type=FileType('r'))
args = parser.parse_args()

config_parser = ConfigParser()
config_parser.read_file(args.config_file)
config = dict(config_parser['default'])
print(config)

t = threading.Thread(target=bitcoin, args=[config], daemon=True)
t.start()

# t2 = threading.Thread(target=dogecoin, args=[config], daemon=True)
# t2.start()

signal.signal(signal.SIGINT, signal_handler)
busy_wait()