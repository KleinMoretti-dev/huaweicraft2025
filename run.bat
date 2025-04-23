g++ src\main.cpp src\solution.cpp -o code_craft.exe -O2 -std=c++17 -pthread -DDHXH
python3 run.py interactor\windows\interactor-live.exe data\sample_official_1.in code_craft.exe