# Contributing to Hyperedge Inclusion Forest

Thank you for your interest in contributing!

## How to Contribute

### Reporting Issues
- Use GitHub Issues to report bugs
- Include minimal reproducible example
- Specify your compiler and OS

### Pull Requests
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Run tests: `make test`
5. Run benchmarks: `make bench`
6. Commit your changes (`git commit -m 'Add amazing feature'`)
7. Push to branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

### Code Style
- Follow existing C style (K&R-ish)
- Use meaningful variable names
- Comment complex algorithms
- Keep functions under 50 lines when possible

### Testing
- Add tests for new features
- Ensure all existing tests pass
- Benchmark performance if relevant

### Areas for Contribution

#### High Priority
1. **Balancing strategies** - AVL/Red-Black rotations
2. **Parallel insertion** - Multi-threaded construction
3. **Query optimization** - Better pruning strategies
4. **Real-world benchmarks** - FIMI datasets, biological networks

#### Medium Priority
1. **Language bindings** - Python, Rust, Go wrappers
2. **Persistent structures** - Immutable version
3. **Serialization** - Save/load forest to disk
4. **Visualization** - Graphviz export

#### Research
1. **Formal complexity proofs** - Tighten bounds
2. **α-nesting characterization** - Empirical study on real graphs
3. **Worst-case optimization** - Amortized guarantees
4. **Cache efficiency** - Memory layout optimization

## Questions?

Open a discussion on GitHub or email the maintainer.
