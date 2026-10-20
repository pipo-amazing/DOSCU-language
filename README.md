# DOSCU Language สำหรับ Visual Studio Code

ส่วนขยายนี้เพิ่มสี syntax สำหรับไฟล์ `.doscu` พร้อมคำสั่ง **DOSCU: Run Current File** โดยมี runtime ภาษา DOSCU (C++17) และตัวเรียกใช้งาน Python อยู่ในส่วนขยาย

## สิ่งที่ต้องติดตั้ง

- Visual Studio Code 1.80 ขึ้นไป
- Python 3
- คอมไพเลอร์ C++17 อย่าง `clang++`, `g++`, `c++` หรือ Microsoft `cl`

บน macOS ให้ติดตั้ง Apple Command Line Tools หากยังไม่มี `clang++` ส่วน Windows ให้เปิด VS Code จาก Visual Studio Developer Command Prompt เพื่อให้ใช้ `cl` ได้ หากระบบหา compiler หรือ Python ไม่พบ ให้กำหนด `CXX` หรือค่า `doscu.pythonPath` ใน Settings ของ VS Code

## ติดตั้งส่วนขยาย

### ติดตั้งจากไฟล์ VSIX

1. ติดตั้ง Node.js และ npm
2. เปิด Terminal ในโฟลเดอร์ส่วนขยาย
3. สร้างไฟล์ติดตั้ง:

   ```sh
   npx @vscode/vsce package
   ```

4. ใน VS Code เปิด Command Palette ด้วย `Cmd+Shift+P` (macOS) หรือ `Ctrl+Shift+P` (Windows/Linux)
5. เลือก **Extensions: Install from VSIX...** แล้วเลือกไฟล์ `.vsix` ที่สร้างขึ้น
6. เปิดหรือสร้างไฟล์นามสกุล `.doscu`

### ทดลองพัฒนาส่วนขยาย

เปิดโฟลเดอร์ส่วนขยายใน VS Code แล้วกด `F5` เพื่อเปิด Extension Development Host จากนั้นเปิดตัวอย่าง `examples/hello.doscu` ในหน้าต่างใหม่

## เริ่มเขียนโปรแกรม DOSCU

สร้างไฟล์ `hello.doscu` แล้วใส่โค้ดนี้:

```doscu
#head() {
    int score = 0;

    int add(int left, int right) {
        return left + right;
    }
}

#part("เริ่มต้น", true) {
    score = add(score, 2);
    cout << "คะแนน: " << score << endl;
    wait(250);
    cout << "เสร็จแล้ว" << endl;
}
```

บันทึกไฟล์ จากนั้นเปิด Command Palette แล้วเรียก **DOSCU: Run Current File** หรือกดปุ่ม Run ที่แถบชื่อ editor ผลลัพธ์และข้อผิดพลาดจาก compiler จะแสดงใน Output panel ช่อง **DOSCU**

## โครงสร้างโปรแกรม

### ส่วน `#head()`

ใช้ประกาศตัวแปร global และฟังก์ชันที่ต้องการใช้ร่วมกัน เช่น:

```doscu
#head() {
    int count = 0;

    int doubleValue(int value) {
        return value * 2;
    }
}
```

### ส่วน `#part("ชื่อ", เปิดใช้งาน)`

ใช้เขียนคำสั่งที่ต้องการให้ทำงาน แต่ละ part ทำงานตามลำดับที่ปรากฏในไฟล์ และ part ที่กำหนด `false` จะไม่ทำงาน:

```doscu
#part("แสดงผล", true) {
    count = doubleValue(4);
    cout << count << endl;
}

#part("ส่วนที่ปิดไว้", false) {
    cout << "บรรทัดนี้จะไม่ทำงาน" << endl;
}
```

ไฟล์ที่ถูก include สามารถมี global และฟังก์ชันได้ แต่ห้ามประกาศ part

### นำเข้าไฟล์

ใช้ `#include` กับไฟล์ DOSCU ในโฟลเดอร์เดียวกัน:

```doscu
#include "helpers.doscu"
```

## Syntax ที่รองรับ

ชนิดข้อมูลที่ประกาศได้: `int`, `float`, `double`, `bool`, `string`, `char` และ `auto`

คำสั่งที่รองรับประกอบด้วยการประกาศและกำหนดค่า, block, `if`/`else`, `while`, `break`, `continue`, ฟังก์ชันและ `return`, การพิมพ์ด้วย `cout << ... << endl`, `wait(ms)` และคำสั่งว่าง ทุกคำสั่งต้องปิดท้ายด้วย `;` รองรับ comment แบบ `//` และ `/* ... */`

ตัวดำเนินการและนิพจน์ที่รองรับ:

- ค่าตัวเลข, ข้อความ, boolean, ตัวแปร และการเรียกฟังก์ชัน
- unary `!`, `+`, `-` และ factorial แบบ postfix เช่น `5!`
- คณิตศาสตร์ `+`, `-`, `*`, `/`, `%` และยกกำลัง `^`
- `base ^? value` สำหรับ log และ `value ^/ degree` สำหรับราก
- เปรียบเทียบค่า, `&&`/`and`, `||`/`or` และ `xor`
- เขียนเปอร์เซ็นต์ติดกับตัวเลขเพื่อแปลงเป็นเศษส่วน เช่น `25%` เท่ากับ `0.25`
- ค่าคงที่ `pi` และ `e`

ฟังก์ชันที่มีให้: `abs`, `round`, `floor`, `ceil`, `sqrt`, `sin`, `cos`, `tan`, `log`, `log.2`, `log.10`, `bit.not`, `bit.and`, `bit.or`, `print`, `out.push`, `out.report`, `wait` และ `stop`

## แก้ปัญหาเบื้องต้น

- **ไม่พบ Python:** ตั้งค่า `doscu.pythonPath` เป็น path ของ Python 3 ใน VS Code Settings
- **ไม่พบ compiler:** ติดตั้ง compiler C++17 หรือกำหนด environment variable `CXX` ให้ชี้ไปยัง compiler
- **Windows หา `cl` ไม่พบ:** เปิด VS Code จาก Developer Command Prompt ของ Visual Studio
- **ไม่เห็นผลลัพธ์:** ตรวจว่าไฟล์บันทึกแล้วและลงท้ายด้วย `.doscu` จากนั้นตรวจ Output panel ช่อง **DOSCU**
- **โปรแกรมหยุดด้วย syntax error:** ตรวจวงเล็บปีกกา เครื่องหมายคำพูด และ `;` ท้ายคำสั่ง

ตัว runner จะคอมไพล์ runtime ไปยังโฟลเดอร์ชั่วคราวทุกครั้งที่รัน จึงไม่แก้ไขไฟล์ต้นฉบับและไม่ทิ้ง executable ไว้ในโฟลเดอร์ส่วนขยาย

## ทดสอบ runtime

จากโฟลเดอร์ส่วนขยาย รัน:

```sh
python3 -m unittest discover -s tests
```

ชุดทดสอบจะคอมไพล์ runtime และตรวจ syntax ที่เอกสารอธิบาย รวมถึงตัวเรียกไฟล์

---

# DOSCU Language for Visual Studio Code

This extension adds syntax highlighting for `.doscu` files and the **DOSCU: Run Current File** command. It bundles the DOSCU C++17 runtime and a Python launcher.

## Requirements

- Visual Studio Code 1.80 or later
- Python 3
- A C++17 compiler: `clang++`, `g++`, `c++`, or Microsoft `cl`

On macOS, install Apple Command Line Tools if `clang++` is unavailable. On Windows, launch VS Code from a Visual Studio Developer Command Prompt so `cl` is available. If automatic detection does not find your tools, set `CXX` to the compiler command or configure `doscu.pythonPath` in VS Code settings.

## Install the extension

### Install from a VSIX package

1. Install Node.js and npm.
2. Open a terminal in the extension folder.
3. Build the installable package:

   ```sh
   npx @vscode/vsce package
   ```

4. In VS Code, open the Command Palette with `Cmd+Shift+P` (macOS) or `Ctrl+Shift+P` (Windows/Linux).
5. Select **Extensions: Install from VSIX...** and choose the generated `.vsix` file.
6. Open or create a file with the `.doscu` extension.

### Run the extension in development

Open the extension folder in VS Code and press `F5` to launch an Extension Development Host. Open `examples/hello.doscu` in that new window to try it.

## Write and run your first DOSCU program

Create `hello.doscu` with this code:

```doscu
#head() {
    int score = 0;

    int add(int left, int right) {
        return left + right;
    }
}

#part("Getting started", true) {
    score = add(score, 2);
    cout << "Score: " << score << endl;
    wait(250);
    cout << "Done" << endl;
}
```

Save the file, open the Command Palette, and run **DOSCU: Run Current File**. You can also use the Run button in the editor title. Program output and compiler errors appear in the **DOSCU** channel of the Output panel.

## Program structure

### The `#head()` section

Use `#head()` to declare global variables and functions that can be shared by parts:

```doscu
#head() {
    int count = 0;

    int doubleValue(int value) {
        return value * 2;
    }
}
```

### The `#part("name", enabled)` section

Write executable statements inside a part. Parts run in source order. A part marked `false` is skipped:

```doscu
#part("Show result", true) {
    count = doubleValue(4);
    cout << count << endl;
}

#part("Disabled section", false) {
    cout << "This line will not run" << endl;
}
```

Included files can define globals and functions, but cannot define parts.

### Include another file

Use `#include` to load a DOSCU file from the same folder:

```doscu
#include "helpers.doscu"
```

## Supported syntax

Supported declaration types are `int`, `float`, `double`, `bool`, `string`, `char`, and `auto`.

Supported statements include declarations and assignments, blocks, `if`/`else`, `while`, `break`, `continue`, functions and `return`, output with `cout << ... << endl`, `wait(ms)`, and empty statements. End statements with semicolons. Both `//` and `/* ... */` comments are supported.

Supported operators and expressions:

- Numeric, string, and boolean literals; variables and function calls
- Unary `!`, `+`, and `-`, plus postfix factorial such as `5!`
- Arithmetic `+`, `-`, `*`, `/`, `%`, and exponentiation `^`
- `base ^? value` for logarithms and `value ^/ degree` for roots
- Comparisons, `&&`/`and`, `||`/`or`, and `xor`
- A percent sign adjacent to a number converts it to a fraction: `25%` evaluates to `0.25`
- Constants `pi` and `e`

Available built-ins: `abs`, `round`, `floor`, `ceil`, `sqrt`, `sin`, `cos`, `tan`, `log`, `log.2`, `log.10`, `bit.not`, `bit.and`, `bit.or`, `print`, `out.push`, `out.report`, `wait`, and `stop`.

## Troubleshooting

- **Python was not found:** Set `doscu.pythonPath` to your Python 3 executable in VS Code settings.
- **No compiler was found:** Install a C++17 compiler or set the `CXX` environment variable to the compiler command.
- **Windows cannot find `cl`:** Start VS Code from the Visual Studio Developer Command Prompt.
- **No output appears:** Save the file, check that its name ends in `.doscu`, and open the **DOSCU** channel in the Output panel.
- **A syntax error appears:** Check braces, quotation marks, and the semicolon at the end of each statement.

The runner compiles the runtime into a temporary directory each time you run a file. It does not modify source files or leave executables in the extension folder.

## Test the runtime

From the extension folder, run:

```sh
python3 -m unittest discover -s tests
```

The test suite compiles the runtime and checks the documented syntax and file runner.
