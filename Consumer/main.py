import sys
import signal
from random import choice
from argparse import ArgumentParser, FileType
from configparser import ConfigParser
from time import sleep
from confluent_kafka import Producer
import threading
import json

def load_topics(config):
    with open(config['topics_file']) as f:
        return json.load(f)

def run_consumer(config, topic):
    topic_name = topic['Name']
    topic_acronym = topic['Acronym']
    print(f'Starting consumer for topic {topic_name} - {topic_acronym}')
    sleep(10)

def spin_consumers(config, topics):
    for topic in topics:
        t = threading.Thread(target=run_consumer, args=(config, topic), daemon=True)
        t.start()

def busy_wait():
    while True:
        sleep(1)

def signal_handler(signum, frame):
    res = input("Ctrl-c was pressed. Do you really want to exit? y/n ")
    if res == 'y':
        exit(1)

if __name__ == '__main__':
    parser = ArgumentParser()
    parser.add_argument('-c', '--config', type=FileType('r'), required=True)
    parser.add_argument('-t', '--topics_file', type=FileType('r'), required=True)
    args = parser.parse_args()

    config_parser = ConfigParser()
    config_parser.read_file(args.config)
    config = dict(config_parser['default'])
    print(config)

    topics : dict = json.load(args.topics_file)
    print(topics["Coins"])
 
    signal.signal(signal.SIGINT, signal_handler)
    spin_consumers(config, topics["Coins"])
    busy_wait()