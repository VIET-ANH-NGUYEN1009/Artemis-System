import sys
import os
from serial.tools import list_ports

# Nhập thư viện esptool và các hàm custom luồng xuất dữ liệu
import esptool

from PySide6.QtCore import QThread, Signal, Slot
from PySide6.QtWidgets import (
    QApplication,
    QWidget,
    QPushButton,
    QLabel,
    QLineEdit,
    QFileDialog,
    QTextEdit,
    QVBoxLayout,
    QHBoxLayout,
    QComboBox,
)

# ============================================================
# LỚP HỖ TRỢ: Bắt dữ liệu print từ esptool truyền về giao diện
# ============================================================
class EsptoolOutputRedirector:
    def __init__(self, signal_to_emit):
        self.signal = signal_to_emit
        self.buffer = ""

    def write(self, string):
        # esptool sử dụng cả \r (đè dòng) và \n (xuống dòng)
        if '\r' in string:
            parts = string.split('\r')
            for part in parts[:-1]:
                if part.strip():
                    self.signal.emit(part + "\r")
            self.buffer = parts[-1]
        elif '\n' in string:
            parts = string.split('\n')
            self.buffer += parts[0]
            if self.buffer.strip():
                self.signal.emit(self.buffer)
            for part in parts[1:-1]:
                if part.strip():
                    self.signal.emit(part)
            self.buffer = parts[-1]
        else:
            self.buffer += string

    def flush(self):
        if self.buffer.strip():
            self.signal.emit(self.buffer)
            self.buffer = ""


# ============================================================
# WORKER THREAD: Gọi esptool trực tiếp bằng hàm nội bộ
# ============================================================
class FlashWorker(QThread):
    log_signal = Signal(str)
    finished_signal = Signal(bool, str)

    def __init__(self, cmd_args):
        super().__init__()
        self.cmd_args = cmd_args

    def run(self):
        # Chuyển hướng stdout hệ thống của luồng này sang bộ lọc custom
        old_stdout = sys.stdout
        redirector = EsptoolOutputRedirector(self.log_signal)
        sys.stdout = redirector
        
        try:
            # Gọi trực tiếp hàm main của thư viện esptool trong luồng phụ
            esptool.main(self.cmd_args)
            self.finished_signal.emit(True, "========== PASS ==========")
        except Exception as e:
            self.finished_signal.emit(False, f"========== FAIL ==========\n{str(e)}")
        finally:
            # Trả lại stdout mặc định cho hệ thống sau khi chạy xong
            sys.stdout = old_stdout


# ============================================================
# GIAO DIỆN CHÍNH
# ============================================================
class FlashTool(QWidget):

    def __init__(self):
        super().__init__()

        self.setWindowTitle("ESP32 Multi-Chip Flash Tool (Fixed EXE)")
        self.resize(750, 480)

        self.path = QLineEdit()
        self.comBox = QComboBox()
        
        self.chipBox = QComboBox()
        self.chipBox.addItems(["esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6", "esp32c5"])
        self.chipBox.setCurrentText("esp32c5")

        btnBrowse = QPushButton("Browse")
        btnRefresh = QPushButton("Refresh")
        self.btnFlash = QPushButton("Flash")
        self.btnFlash.setStyleSheet("background-color: #007acc; color: white; font-weight: bold; font-size: 14px; padding: 6px;")

        self.log = QTextEdit()
        self.log.setReadOnly(True)
        self.log.setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: Consolas, Monaco, monospace; font-size: 12px;")

        layout = QVBoxLayout()

        h1 = QHBoxLayout()
        h1.addWidget(QLabel("Firmware:  "))
        h1.addWidget(self.path)
        h1.addWidget(btnBrowse)

        h2 = QHBoxLayout()
        h2.addWidget(QLabel("COM Port:  "))
        h2.addWidget(self.comBox)
        h2.addWidget(btnRefresh)
        h2.addWidget(QLabel("  Chip Type: "))
        h2.addWidget(self.chipBox)

        layout.addLayout(h1)
        layout.addLayout(h2)
        layout.addWidget(self.btnFlash)
        layout.addWidget(self.log)

        self.setLayout(layout)

        btnBrowse.clicked.connect(self.browse)
        btnRefresh.clicked.connect(self.load_ports)
        self.btnFlash.clicked.connect(self.flash)

        self.load_ports()
        self.worker = None

    def load_ports(self):
        self.comBox.clear()
        ports = list_ports.comports()
        for p in ports:
            self.comBox.addItem(f"{p.device} - {p.description}", p.device)
        if self.comBox.count() == 0:
            self.log.append("Không tìm thấy cổng COM.")

    def browse(self):
        file, _ = QFileDialog.getOpenFileName(self, "Select BIN Firmware", "", "BIN Files (*.bin)")
        if file:
            self.path.setText(file)

    @Slot(str)
    def update_log_realtime(self, text_line):
        """Cập nhật đè chữ khi nhận diện % tiến độ hoặc in dòng mới"""
        # Nếu chuỗi kết thúc bằng \r hoặc chứa từ khóa tiến độ, thực hiện xóa dòng cũ để đè % mới lên
        if text_line.endswith("\r") or "Writing at" in text_line or "bytes..." in text_line:
            clean_text = text_line.replace("\r", "").strip()
            cursor = self.log.textCursor()
            cursor.movePosition(cursor.MoveOperation.End)
            cursor.select(cursor.SelectionType.LineUnderCursor)
            cursor.removeSelectedText()
            cursor.insertText(clean_text)
        else:
            self.log.append(text_line)
        
        self.log.ensureCursorVisible()

    def flash(self):
        if self.worker and self.worker.isRunning():
            return

        binfile = self.path.text().strip()
        if not binfile:
            self.log.append("Chưa chọn file firmware (.bin).")
            return

        if self.comBox.currentIndex() < 0:
            self.log.append("Chưa chọn cổng COM.")
            return

        port = self.comBox.currentData()
        chip = self.chipBox.currentText()

        self.log.clear()
        self.log.append("--------------------------------------------------")
        self.log.append(f"Chip : {chip.upper()}")
        self.log.append(f"Port : {port}")
        self.log.append(f"BIN  : {binfile}")
        self.log.append("--------------------------------------------------")

        filename = os.path.basename(binfile)
        cmd_args = ["--chip", chip, "--port", port, "--baud", "921600", "write_flash"]

        if filename.endswith(".merged.bin"):
            self.log.append("Phát hiện firmware dạng MERGED (đã gộp sẵn).")
            cmd_args.extend(["0x0000", binfile])
        else:
            if filename.endswith(".bootloader.bin") or filename.endswith(".partitions.bin"):
                self.log.append("[HỦY BỎ] Vui lòng chọn file firmware chính (*.bin)")
                return

            folder = os.path.dirname(binfile)
            app = filename[:-4]

            bootloader = os.path.join(folder, app + ".bootloader.bin")
            partitions = os.path.join(folder, app + ".partitions.bin")

            if not os.path.exists(bootloader) or not os.path.exists(partitions):
                self.log.append(f"[LỖI] Không tìm thấy file đi kèm (.bootloader.bin / .partitions.bin)")
                return

            self.log.append("Phát hiện cấu trúc firmware 3 file liên kết.")
            boot_offset = "0x1000" if chip == "esp32" else "0x0000"
            cmd_args.extend([boot_offset, bootloader, "0x8000", partitions, "0x10000", binfile])

        self.btnFlash.setEnabled(False)
        self.log.append("\n[TIẾN HÀNH] Đang kết nối mạch...")
        
        # Khởi chạy luồng phụ an toàn với PyInstaller
        self.worker = FlashWorker(cmd_args)
        self.worker.log_signal.connect(self.update_log_realtime)
        self.worker.finished_signal.connect(self.on_flash_finished)
        self.worker.start()

    def on_flash_finished(self, success, message):
        self.log.append("")
        self.log.append(message)
        self.btnFlash.setEnabled(True)
        self.worker = None


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = FlashTool()
    window.show()
    sys.exit(app.exec())
