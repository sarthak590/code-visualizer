# C++ Code Visualizer

A web-based tool to visualize execution of C++ code step-by-step.

## ✨ Features

- Step-by-step execution
- Line highlighting
- Memory state tracking
- Supports:
  - Variables
  - if-else
  - while loops
  - for loops
- Clean UI with React + Bootstrap

## 🛠 Tech Stack

- Frontend: HTML, React (CDN), Bootstrap
- Backend: C++ (Crow framework)

## ⚙️ How to Run

### Backend

```bash
cd backend
g++ main.cpp -o server -std=c++17 -I./include -I/opt/homebrew/include -pthread
./server

###  Notes

* This is a simplified interpreter (not full C++ compiler)
* Designed for learning and visualization