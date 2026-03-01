# Java Minifier (C++)

Минификатор Java-кода:
- удаляет комментарии `//` и `/* ... */`
- удаляет лишние пробелы и переносы строк
- сохраняет корректность строк/символов

## Build
```bash
mkdir -p build
cd build
cmake ..

./java_minifier ../tests/input1.java out.java
./java_minifier --aggressive ../tests/input2.java out.java
```