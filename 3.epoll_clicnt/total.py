import pandas as pd
import matplotlib.pyplot as plt
import os
import time

# CSV 파일 경로 리스트
log_file_paths = [
    '/Users/yunsihun/3학년 1학기/운영체제/share/gazua/epoll_clicnt/broadcast_log1.csv',
    '/Users/yunsihun/3학년 1학기/운영체제/share/gazua/epoll_clicnt/broadcast_log2.csv',
    '/Users/yunsihun/3학년 1학기/운영체제/share/gazua/epoll_clicnt/broadcast_log3.csv'
]

# 각 파일에 대한 레이블 (그래프 범례에 사용)
labels = ['libwebsocket', 'multhread', 'epoll', 'final']

# 모든 로그 파일이 생성될 때까지 대기
while not all(os.path.exists(path) for path in log_file_paths):
    print("하나 이상의 로그 파일을 찾을 수 없습니다. 서버가 실행 중인지 확인해주세요.")
    time.sleep(5)  # 5초 대기 후 다시 확인

# 데이터를 저장할 리스트
grouped_data = []

for path in log_file_paths:
    try:
        df = pd.read_csv(path)
        print(f"{path} 데이터 미리보기:")
        print(df.head())
        
        # 'client_count'별 평균 'elapsed_time_ms' 계산
        grouped_df = df.groupby('client_count')['elapsed_time_ms'].mean().reset_index()
        grouped_data.append(grouped_df)
    except Exception as e:
        print(f"CSV 파일 {path}을(를) 읽는 도중 오류 발생: {e}")
        exit(1)

# 그래프 생성
plt.figure(figsize=(12, 6))

# 색상 리스트 (필요에 따라 추가하거나 변경 가능)
colors = ['red', 'orange', 'green', 'purple']

for i, grouped_df in enumerate(grouped_data):
    plt.plot(
        grouped_df['client_count'],
        grouped_df['elapsed_time_ms'],
        marker='o',
        color=colors[i],
        label=labels[i]
    )

plt.xlabel('Client Count')
plt.ylabel('Elapsed Time (ms)')
plt.title('Elapsed Time vs Client Count for Multiple Logs')
plt.grid(True)
plt.ylim(0, 10)  # Y축 범위 설정
plt.legend()      # 범례 추가
plt.tight_layout()
plt.savefig('elapsed_time_vs_client_count.png')  # 그래프를 이미지 파일로 저장
plt.show()
