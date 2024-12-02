#!/bin/bash

# 클라이언트 10개 실행
for i in {1..10}
do
    ./client &  # 클라이언트를 백그라운드에서 실행
    sleep 1
done

wait  # 모든 백그라운드 프로세스가 종료될 때까지 대기
a