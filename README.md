# bfj
Another brainfuck optimizing JIT compiler, but its written in under 200 lines
of C with no dependencies.

## Building
You can use make.

```bash
make
```

## Usage
Pass the path of program as an argument.

```bash
bfj tests/mandelbrot.bf
```

## Limitation
Does not support Windows and can buffer overflow. ¯\\_(ツ)_/¯

## License
MIT License.
