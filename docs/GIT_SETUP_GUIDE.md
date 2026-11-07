# Hướng Dẫn Thiết Lập & Quản Lý Git Repository Chuẩn

Tài liệu này hướng dẫn chi tiết cách quản lý mã nguồn dự án **ESP32-C6 Smart Home Central Controller** bằng Git theo chuẩn công nghiệp (Git Flow thu gọn).

---

## 1. Cấu Trúc Nhánh (Branching Strategy)

Dự án sử dụng mô hình 2 nhánh dài hạn kết hợp các nhánh tính năng:

* **`main`**: Nhánh chính chứa mã nguồn đã kiểm thử ổn định (Production-ready). Các bản phát hành chính thức (`v2.1.0`) được gắn thẻ (Tag) tại đây.
* **`develop`**: Nhánh tích hợp thường trực dùng cho quá trình phát triển hàng ngày. Mọi module sau khi viết và kiểm thử sẽ được merge vào đây trước khi đưa lên `main`.
* **`feature/phase-1-bringup`**: Nhánh cho Phase 1 (Nối mạch, kiểm tra sụt áp brownout, GPIO).
* **`feature/phase-2-drivers`**: Nhánh cho Phase 2 (I2C Manager, SHT31, SSD1306, ADC LDR, UART PMS7003 FSM).
* **`feature/phase-3-timekeeping`**: Nhánh cho Phase 3 (Monotonic Timer Engine, SNTP Sync, Cron Scheduler).
* **`feature/phase-4-integration`**: Nhánh cho Phase 4 (Rule Engine, USB Telemetry, Python Test Harness).

```
main       ●───────────────────────────────────────────────────────● (Release v2.1.0)
           │                                                       ▲
           │                                                       │ Merge
develop    └──●───────●───────────────●───────────────●────────────┤
              │       ▲               ▲               ▲
              │ Phase1│ Phase 2       │ Phase 3       │ Phase 4
              └───●───┘       └───●───┘       └───●───┘
```

---

## 2. Các Bước Khởi Tạo Repo & Commit Trên Máy Trạm

### Bước 2.1: Cấu hình thông tin cá nhân (Nếu chưa thiết lập)
Mở PowerShell trên máy và chạy:
```powershell
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
```

### Bước 2.2: Khởi tạo Git và thêm file
Tại thư mục gốc dự án (`c:\file_BK\LearninAI\Projec2`):
```powershell
cd c:\file_BK\LearninAI\Projec2

# Khởi tạo repository
git init

# Kiểm tra danh sách file (thư mục build/ đã được .gitignore tự động bỏ qua)
git status

# Thêm tất cả mã nguồn và tài liệu vào Stage
git add .

# Thực hiện commit đầu tiên
git commit -m "feat: initial commit of ESP32-C6 Smart Home Central Controller v2.1 (Pure C + Python)"
```

### Bước 2.3: Đổi tên nhánh mặc định thành `main` và tạo nhánh `develop`
```powershell
git branch -M main
git checkout -b develop
```

---

## 3. Hướng Dẫn Đẩy Lên GitHub / GitLab Cá Nhân

### Bước 3.1: Tạo Repo mới trên GitHub
1. Đăng nhập vào [GitHub](https://github.com) -> Nhấn dấu **`+`** ở góc phải -> Chọn **New repository**.
2. Đặt tên: `ESP32-C6-Smart-Home-Controller`.
3. Chọn chế độ **Private** (Riêng tư) hoặc **Public** (Công khai) tùy bạn.
4. **Lưu ý:** KHÔNG tích chọn "Add a README file" hay ".gitignore" (vì trong project đã có sẵn đầy đủ).
5. Nhấn **Create repository**.

### Bước 3.2: Liên kết Remote và đẩy mã nguồn lên
Copy đường link repo vừa tạo (dạng HTTPS hoặc SSH) và chạy lệnh:
```powershell
# Thêm Remote origin (thay URL bên dưới bằng URL repo GitHub của bạn)
git remote add origin https://github.com/<your-username>/ESP32-C6-Smart-Home-Controller.git

# Đẩy cả 2 nhánh main và develop lên GitHub
git push -u origin main
git push -u origin develop
```

---

## 4. Các Lệnh Git Hàng Ngày Khi Làm Việc

| Tác vụ | Câu lệnh Git |
| :--- | :--- |
| **Xem trạng thái thay đổi** | `git status` |
| **Xem lịch sử commit** | `git log --oneline --graph --decorate` |
| **Tạo nhánh làm Phase mới** | `git checkout -b feature/phase-2-drivers` |
| **Lưu công việc sau khi xong 1 module** | `git add .`<br>`git commit -m "feat(i2c): add 9-pulse bus recovery and static mutex"` |
| **Gộp nhánh về develop** | `git checkout develop`<br>`git merge feature/phase-2-drivers` |
| **Đẩy code lên GitHub** | `git push origin develop` |
| **Tạo Tag bản phát hành chính thức** | `git checkout main`<br>`git merge develop`<br>`git tag -a v2.1.0 -m "Release v2.1.0: Pure C & Python Certified"`<br>`git push origin main --tags` |

---

## 5. Quy Ước Đặt Tên Commit (Conventional Commits)

Để lịch sử git rõ ràng, chuyên nghiệp:
* `feat:` Tính năng mới (ví dụ: `feat(uart): implement 5-state FSM parser`)
* `fix:` Sửa lỗi (ví dụ: `fix(i2c): adjust SCL recovery pulse timing`)
* `docs:` Cập nhật tài liệu (ví dụ: `docs: update PRD and work split roadmap`)
* `test:` Bổ sung bài test (ví dụ: `test(harness): add automated reboot detection test`)
* `refactor:` Tái cấu trúc thư mục hoặc code mà không đổi chức năng.
