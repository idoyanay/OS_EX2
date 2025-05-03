import subprocess
import os

# Configuration
lib_path = "libuthreads.a"
include_flags = "-I."
compile_flags = "-std=c++11"
link_flags = "-lpthread"
tests = [f"Tests/test{i}" for i in range(1, 9)]  # test1 to test8

def compile_test(test_name):
    cpp_file = f"{test_name}.cpp"
    exe_file = test_name
    print(f"\nCompiling {cpp_file}...")

    if not os.path.exists(cpp_file):
        print(f"{cpp_file} not found ❌")
        return False

    cmd = f"g++ {compile_flags} {include_flags} {cpp_file} {lib_path} {link_flags} -o {exe_file}"
    try:
        subprocess.run(cmd, shell=True, check=True)
        print(f"{test_name} compiled successfully ✅")
        return True
    except subprocess.CalledProcessError:
        print(f"Compilation failed for {test_name} ❌")
        return False

def run_test(test_name):
    print(f"\nRunning {test_name}...")
    try:
        subprocess.run(f"./{test_name}", check=True)
        print(f"{test_name} PASSED ✅")
        return True
    except subprocess.CalledProcessError:
        print(f"{test_name} FAILED ❌")
        return False
    except FileNotFoundError:
        print(f"{test_name} not found! ❌")
        return False

def main():
    passed = 0
    failed = 0

    print("==== Building and Running All Tests ====")
    for test in tests:
        if compile_test(test):
            if run_test(test):
                passed += 1
            else:
                failed += 1
        else:
            failed += 1

    print("\n==== Test Summary ====")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    if failed == 0:
        print("🎉 All tests passed!")
    else:
        print("❗ Some tests failed.")

if __name__ == "__main__":
    main()
