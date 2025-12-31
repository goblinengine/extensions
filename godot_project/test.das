func test_operators()
	print("\n--- Testing Operators ---")
	// Arithmetic
	print("10 + 5 = {10 + 5}")
	print("10 - 5 = {10 - 5}")
	print("10 * 5 = {10 * 5}")
	print("10 / 5 = {10 / 5}")
	print("10 % 3 = {10 % 3}")
	print("2 ** 3 = {pow(2.0, 3.0)}")
	
	// Comparison
	print("10 > 5: {10 > 5}")
	print("10 < 5: {10 < 5}")
	print("10 >= 10: {10 >= 10}")
	print("10 <= 10: {10 <= 10}")
	print("10 == 10: {10 == 10}")
	print("10 != 5: {10 != 5}")
	
	// Logical
	print("true and false: {true && false}")
	print("true or false: {true || false}")
	print("not true: {!true}")
	
	// Bitwise
	print("5 & 3: {5 & 3}")
	print("5 | 3: {5 | 3}")
	print("5 ^ 3: {5 ^ 3}")
	print("~5: {~5}")
	print("4 << 1: {4 << 1}")
	print("4 >> 1: {4 >> 1}")

func _ready()
	print("das tests")
	test_operators()
