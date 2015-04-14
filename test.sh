#!/bin/bash

./server 9649 20000 &
PID1=$!
sleep 1
read -p "Press enter to run test1"
./client 9649 20000 15 100 127.0.0.1 1 1
read -p "Press enter to run test2"
./client 9649 20000 15 100 127.0.0.1 2 2
read -p "Press enter to run test3"
./client 9649 20000 15 100 127.0.0.1 3 3
read -p "Press enter to run test4"
./client 9649 20000 30 50 127.0.0.1 1 4
read -p "Press enter to run test5"
./client 9649 20000 30 50 127.0.0.1 2 5
read -p "Press enter to run test6"
./client 9649 20000 30 50 127.0.0.1 3 6
read -p "Press enter to run test7"
./client 9649 20000 60 25 127.0.0.1 1 7
read -p "Press enter to run test8"
./client 9649 20000 60 25 127.0.0.1 2 8
read -p "Press enter to run test9"
./client 9649 20000 60 25 127.0.0.1 3 9
kill $PID1
