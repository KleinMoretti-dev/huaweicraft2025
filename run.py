# -*- coding: utf-8 -*-
import argparse
import os
import subprocess
import time

DEBUG_LEN = 2
REPLAY_LEN = 10
ROUND_LEN = 1


def convert(arr, name, length):
    if arr is None:
        name = ""
        arr = ["" for i in range(length)]
    else:
        arr = list(map(str, arr))
        arr += ["" for i in range(length - len(arr))]

    return [name] + arr


def main(interactor, data, player, debug, replay, rd):
    data_out = "result.txt"

    debug = convert(debug, "debug", DEBUG_LEN)
    replay = convert(replay, "replay", REPLAY_LEN)
    rd = convert(rd, "round", ROUND_LEN)

    pipe1_read, pipe1_write = os.pipe()
    pipe2_read, pipe2_write = os.pipe()

    process1 = subprocess.Popen([interactor, data, data_out] + debug + replay + rd,
                                stdin=pipe2_read,
                                stdout=pipe1_write)

    process2 = subprocess.Popen(player.split(),
                                stdin=pipe1_read,
                                stdout=pipe2_write)

    process1.wait()
    time.sleep(0.1)
    process2.terminate()
    time.sleep(0.1)

    os.close(pipe1_read)
    os.close(pipe1_write)
    os.close(pipe2_read)
    os.close(pipe2_write)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('interactor', type=str, nargs=1)
    parser.add_argument('data', type=str, nargs=1)
    parser.add_argument('player', type=str, nargs=1)
    # 使用*，没有-d是None，-d没有任何参数是[]
    parser.add_argument('--debug', '-d', type=int, nargs='*')
    parser.add_argument('--replay', '-r', type=int, nargs='*')
    parser.add_argument('--round', type=int, nargs='*')

    args = parser.parse_args()

    if args.debug is not None and len(args.debug) > DEBUG_LEN:
        parser.error("argument --debug/-d: accepts at most 2 arguments")

    if args.replay is not None and len(args.replay) > REPLAY_LEN:
        parser.error("argument --replay/-r: accepts at most 10 arguments")

    if args.round is not None and len(args.round) > ROUND_LEN:
        parser.error("argument --round: accepts at most 1 argument")

    os.makedirs("replay", exist_ok=True)

    main(args.interactor[0], args.data[0], args.player[0], args.debug, args.replay, args.round)
