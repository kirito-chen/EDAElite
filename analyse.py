import matplotlib.pyplot as plt
import sys
def parse_file(filename):
    data = {}
    current_case = None
    iteration_count = 0  # 用于跟踪[INFO] T:出现的次数

    with open(filename, 'r') as file:
        for line in file:
            if "ready to run" in line:
                # 提取案例名称
                current_case = line.strip()
                data[current_case] = []
                iteration_count = 0  # 重置迭代计数
            elif "[INFO] T:" in line and current_case:
                iteration_count += 1
                parts = line.split("cost:")
                if len(parts) > 1:
                    cost = int(parts[1].strip())
                    data[current_case].append((iteration_count, cost))

    return data

def plot_data(data):
    for readycase, values in data.items():
        if values:
            parts = readycase.split(" ")
            case = parts[-1]
            x, y = zip(*values)
            plt.figure()  # 创建一个新的图形
            plt.scatter(x, y, label=case, s=5)
            # plt.plot(x, y)  # 添加折线以更好地可视化趋势
            plt.title(f'Cost vs Iteration for {case}')
            plt.xlabel('Iteration Number')
            plt.ylabel('Cost')
            plt.legend()
            plt.grid()
            plt.savefig(f'0_Result_Graph/SA_280s/{case}.png')  # 保存图像为PNG文件
            
if __name__ == "__main__":
    
    if len(sys.argv) != 2:
        print("usage: phthon analyse.py xxx.log")
        exit();
    filename = sys.argv[1]  # 请替换为你的文件名
    data = parse_file(filename)
    plot_data(data)
