import pandas as pd
import matplotlib.pyplot as plt
import os
import time

# 로그 파일 경로
log_file_path = '/Users/yunsihun/3학년 1학기/운영체제/share/gazua/epoll_clicnt/broadcast_log.csv'

# 로그 파일이 생성될 때까지 대기
while not os.path.exists(log_file_path):
    print(f"{log_file_path} 파일을 찾을 수 없습니다. 서버가 실행 중인지 확인해주세요.")
    time.sleep(5)  # 5초 대기 후 다시 확인

# CSV 파일 읽기
try:
    df = pd.read_csv(log_file_path)
except Exception as e:
    print(f"CSV 파일을 읽는 도중 오류 발생: {e}")
    exit(1)

# 데이터 확인
print(df.head())

# client_count를 기준으로 그룹화하여 elapsed_time_ms의 평균 계산
grouped_df = df.groupby('client_count')['elapsed_time_ms'].mean().reset_index()

# 그래프 생성
plt.figure(figsize=(12, 6))
plt.plot(grouped_df['client_count'], grouped_df['elapsed_time_ms'], marker='o')
plt.xlabel('Client Count')
plt.ylabel('Elapsed Time (ms)')
plt.title('Elapsed Time vs Client Count')
plt.grid(True)
plt.tight_layout()
plt.savefig('elapsed_time_vs_client_count.png')  # 그래프를 이미지 파일로 저장
plt.show()
