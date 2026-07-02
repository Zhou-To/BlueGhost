
# 目标蓝牙MAC地址
TARGET_DEVICE_MAC="XX:XX:XX:XX:XX:XX" 
# 测试循环次数
ITERATIONS=100
# 配对操作的超时时间 (秒)
PAIR_TIMEOUT=25
# 日志文件保存路径
LOG_FILE="bluetooth_pairing_results.csv"

total_duration=0  # 累加成功配对的总耗时
success_count=0   # 统计成功配对的次数


# 写入 CSV 表头
echo "iteration,result,duration_seconds,timestamp" > "$LOG_FILE"
echo "# 蓝牙配对测试启动 | 目标设备MAC: $TARGET_DEVICE_MAC | 总迭代次数: $ITERATIONS | 单次超时: $PAIR_TIMEOUT 秒" >> "$LOG_FILE"
echo "# 测试启动时间: $(date '+%Y-%m-%d %H:%M:%S')" >> "$LOG_FILE"

# 打印控制台启动信息
echo "Starting pairing test with $TARGET_DEVICE_MAC for $ITERATIONS iterations."
echo "Each pairing attempt has a timeout of $PAIR_TIMEOUT seconds."
echo "All time data will be saved to: $LOG_FILE"


for i in $(seq 1 $ITERATIONS)
do
    echo -e "\n--- Iteration $i of $ITERATIONS ---"

    bluetoothctl remove "$TARGET_DEVICE_MAC" > /dev/null 2>&1
    sleep 1 

    #扫描设备
    echo "Scanning for device..."
    (
        echo "scan on"
        sleep 7  # 确保扫描到目标设备
        echo "scan off"
    ) | bluetoothctl > /dev/null

    # 计时与配对阶段
    echo "Attempting to pair..."
    start_time=$(date +%s.%N)  # 记录开始时间
    current_timestamp=$(date '+%Y-%m-%d %H:%M:%S')  # 记录当前时间戳

    pair_output=$(timeout $PAIR_TIMEOUT bluetoothctl pair "$TARGET_DEVICE_MAC")

    # 结果判断与时间计算
    if [ $? -eq 0 ] && echo "$pair_output" | grep -q "Pairing successful"; then
        # 配对成功：计算耗时并更新统计
        end_time=$(date +%s.%N)
        duration=$(echo "$end_time - $start_time" | bc)  # 浮点数计算耗时（秒）

        echo "SUCCESS: Pairing successful in ${duration} seconds."

        echo "$i,success,$duration,$current_timestamp" >> "$LOG_FILE"
        
        # 更新统计计数器
        success_count=$((success_count + 1))
        total_duration=$(echo "$total_duration + $duration" | bc)

    else
        # 配对失败：记录失败状态
        echo "FAILURE: Pairing failed or timed out."
        echo "Output from bluetoothctl:"
        echo "$pair_output"
        echo "$i,failure,N/A,$current_timestamp" >> "$LOG_FILE"
    fi

    #  清理与休息
    bluetoothctl remove "$TARGET_DEVICE_MAC" > /dev/null 2>&1  
    echo "Waiting before next run..."
    sleep 3
done


echo -e "\n----------------------------------------" >> "$LOG_FILE"
echo "# 测试结束时间: $(date '+%Y-%m-%d %H:%M:%S')" >> "$LOG_FILE"
echo "# 总尝试次数: $ITERATIONS" >> "$LOG_FILE"
echo "# 成功次数: $success_count" >> "$LOG_FILE"
echo "# 失败次数: $((ITERATIONS - success_count))" >> "$LOG_FILE"
if [ $success_count -gt 0 ]; then
    average_time=$(echo "scale=4; $total_duration / $success_count" | bc)
    echo "# 成功配对平均耗时: $average_time 秒" >> "$LOG_FILE"
else
    echo "# 成功配对平均耗时: N/A " >> "$LOG_FILE"
fi


# 控制台输出最终统计
echo -e "\n----------------------------------------"
echo "Test finished."
echo "Total attempts: $ITERATIONS"
echo "Successful pairings: $success_count"
echo "Failed pairings: $((ITERATIONS - success_count))"
echo "----------------------------------------"
if [ $success_count -gt 0 ]; then
    echo "Average time for SUCCESSFUL pairings: ${average_time} seconds."
else
    echo "No successful pairings to calculate an average time."
fi
echo -e "\nAll time data has been saved to: $LOG_FILE"