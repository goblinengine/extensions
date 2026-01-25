extends Node

# Test script for comprehensive GDScript feature coverage

# Signals
signal test_signal
signal test_signal_with_args(value: int, name: String)

# Enums
enum Colors { RED, GREEN, BLUE }
enum Flags { FLAG_A = 1, FLAG_B = 2, FLAG_C = 4 }

# Constants
const MAX_HEALTH: int = 100
const PI_APPROX: float = 3.14159
const GAME_NAME: String = "Test Game"

# Exported variables
@export var export_int: int = 42
@export var export_float: float = 3.14
@export var export_string: String = "Hello"
@export_range(0, 100) var export_range_value: int = 50
@export_enum("Option1", "Option2", "Option3") var export_enum_value: String = "Option1"

# Member variables
var int_var: int = 10
var float_var: float = 20.5
var bool_var: bool = true
var string_var: String = "World"
var vector2_var: Vector2 = Vector2(1.0, 2.0)
var vector3_var: Vector3 = Vector3(1.0, 2.0, 3.0)
var array_var: Array = [1, 2, 3, 4, 5]
var dict_var: Dictionary = {"name": "test", "value": 42}
var color_var: Color = Color(1.0, 0.0, 0.0, 1.0)

# Lazy initialization
@onready var lazy_node = get_node(".")

# Static variable (class-level)
static var static_counter: int = 0

func _init():
	print("Constructor called")

func _ready():
	print("=== GDScript Comprehensive Test ===")
	test_variables()
	test_operators()
	test_control_flow()
	test_arrays()
	test_dictionaries()
	test_strings()
	test_math()
	test_vectors()
	test_colors()
	test_functions()
	test_signals_and_callbacks()
	test_enums()
	test_type_checks()
	test_lambdas()
	test_static()
	print("=== All tests complete ===")

func test_variables():
	print("\n--- Testing Variables ---")
	var local_int: int = 100
	var local_float: float = 2.5
	var local_bool: bool = false
	var local_string: String = "local"
	
	print("int: ", local_int)
	print("float: ", local_float)
	print("bool: ", local_bool)
	print("string: ", local_string)
	
	# Type inference
	var inferred = 42
	print("inferred type: ", typeof(inferred))

func test_operators():
	print("\n--- Testing Operators ---")
	# Arithmetic
	print("10 + 5 = ", 10 + 5)
	print("10 - 5 = ", 10 - 5)
	print("10 * 5 = ", 10 * 5)
	print("10 / 5 = ", 10 / 5)
	print("10 % 3 = ", 10 % 3)
	print("2 ** 3 = ", 2 ** 3)
	
	# Comparison
	print("10 > 5: ", 10 > 5)
	print("10 < 5: ", 10 < 5)
	print("10 >= 10: ", 10 >= 10)
	print("10 <= 10: ", 10 <= 10)
	print("10 == 10: ", 10 == 10)
	print("10 != 5: ", 10 != 5)
	
	# Logical
	print("true and false: ", true and false)
	print("true or false: ", true or false)
	print("not true: ", not true)
	
	# Bitwise
	print("5 & 3: ", 5 & 3)
	print("5 | 3: ", 5 | 3)
	print("5 ^ 3: ", 5 ^ 3)
	print("~5: ", ~5)
	print("4 << 1: ", 4 << 1)
	print("4 >> 1: ", 4 >> 1)

func test_control_flow():
	print("\n--- Testing Control Flow ---")
	
	# If/else
	var x = 10
	if x > 5:
		print("x is greater than 5")
	elif x == 5:
		print("x equals 5")
	else:
		print("x is less than 5")
	
	# Ternary
	var result = "positive" if x > 0 else "negative"
	print("ternary result: ", result)
	
	# For loop
	print("for loop:")
	for i in range(5):
		print("  i = ", i)
	
	# For loop with array
	print("for in array:")
	for item in [10, 20, 30]:
		print("  item = ", item)
	
	# While loop
	print("while loop:")
	var counter = 0
	while counter < 3:
		print("  counter = ", counter)
		counter += 1
	
	# Match statement
	print("match statement:")
	var value = 2
	match value:
		1:
			print("  value is 1")
		2:
			print("  value is 2")
		3, 4, 5:
			print("  value is 3, 4, or 5")
		_:
			print("  default case")

func test_arrays():
	print("\n--- Testing Arrays ---")
	var arr: Array = [1, 2, 3, 4, 5]
	print("array: ", arr)
	print("size: ", arr.size())
	print("first: ", arr[0])
	print("last: ", arr[-1])
	
	arr.append(6)
	print("after append: ", arr)
	
	arr.insert(0, 0)
	print("after insert: ", arr)
	
	arr.remove_at(0)
	print("after remove: ", arr)
	
	print("contains 3: ", arr.has(3))
	print("index of 3: ", arr.find(3))
	
	# Array slicing
	print("slice [1:4]: ", arr.slice(1, 4))

func test_dictionaries():
	print("\n--- Testing Dictionaries ---")
	var dict: Dictionary = {
		"name": "Player",
		"health": 100,
		"level": 5,
		"active": true
	}
	print("dictionary: ", dict)
	print("name: ", dict["name"])
	print("health: ", dict.get("health"))
	
	dict["score"] = 1000
	print("after adding score: ", dict)
	
	print("has 'name': ", dict.has("name"))
	print("keys: ", dict.keys())
	print("values: ", dict.values())
	
	dict.erase("active")
	print("after erase: ", dict)

func test_strings():
	print("\n--- Testing Strings ---")
	var str1: String = "Hello"
	var str2: String = "World"
	
	print("concat: ", str1 + " " + str2)
	print("format: ", "Value: %d" % 42)
	print("format multiple: ", "X: %d, Y: %d" % [10, 20])
	print("length: ", str1.length())
	print("uppercase: ", str1.to_upper())
	print("lowercase: ", str2.to_lower())
	print("contains: ", str1.contains("ell"))
	print("begins_with: ", str1.begins_with("Hel"))
	print("ends_with: ", str2.ends_with("rld"))
	print("substr: ", str1.substr(1, 3))
	print("replace: ", str1.replace("Hello", "Hi"))
	print("split: ", "a,b,c".split(","))

func test_math():
	print("\n--- Testing Math ---")
	print("abs(-5): ", abs(-5))
	print("sign(-5): ", sign(-5))
	print("min(3, 7): ", min(3, 7))
	print("max(3, 7): ", max(3, 7))
	print("clamp(15, 0, 10): ", clamp(15, 0, 10))
	print("sqrt(16): ", sqrt(16))
	print("pow(2, 3): ", pow(2, 3))
	print("floor(3.7): ", floor(3.7))
	print("ceil(3.2): ", ceil(3.2))
	print("round(3.5): ", round(3.5))
	print("sin(PI/2): ", sin(PI / 2))
	print("cos(0): ", cos(0))
	print("lerp(0, 10, 0.5): ", lerp(0, 10, 0.5))

func test_vectors():
	print("\n--- Testing Vectors ---")
	var v2a: Vector2 = Vector2(3.0, 4.0)
	var v2b: Vector2 = Vector2(1.0, 2.0)
	
	print("Vector2: ", v2a)
	print("length: ", v2a.length())
	print("normalized: ", v2a.normalized())
	print("dot: ", v2a.dot(v2b))
	print("distance_to: ", v2a.distance_to(v2b))
	print("add: ", v2a + v2b)
	print("subtract: ", v2a - v2b)
	print("multiply: ", v2a * 2.0)
	
	var v3a: Vector3 = Vector3(1.0, 2.0, 3.0)
	var v3b: Vector3 = Vector3(4.0, 5.0, 6.0)
	
	print("Vector3: ", v3a)
	print("cross: ", v3a.cross(v3b))
	print("length: ", v3a.length())

func test_colors():
	print("\n--- Testing Colors ---")
	var color: Color = Color(1.0, 0.5, 0.25, 1.0)
	print("color: ", color)
	print("r: ", color.r)
	print("g: ", color.g)
	print("b: ", color.b)
	print("a: ", color.a)
	print("to_html: ", color.to_html())
	
	var named_color = Color.RED
	print("named color (RED): ", named_color)

func test_functions():
	print("\n--- Testing Functions ---")
	print("add(5, 3): ", add(5, 3))
	print("multiply(4, 2): ", multiply(4, 2))
	print("greet('Alice'): ", greet("Alice"))
	
	# Default parameters
	print("default params: ", func_with_defaults())
	print("override one param: ", func_with_defaults(20))
	print("override both: ", func_with_defaults(20, 5))
	
	# Return multiple values (using array)
	var results = get_stats()
	print("multiple returns: ", results)

func add(a: int, b: int) -> int:
	return a + b

func multiply(a: int, b: int) -> int:
	return a * b

func greet(name: String) -> String:
	return "Hello, " + name + "!"

func func_with_defaults(a: int = 10, b: int = 20) -> int:
	return a + b

func get_stats() -> Array:
	return [100, 50, 75]  # health, mana, stamina

func test_signals_and_callbacks():
	print("\n--- Testing Signals ---")
	
	# Connect signal
	test_signal.connect(_on_test_signal)
	test_signal_with_args.connect(_on_test_signal_with_args)
	
	# Emit signals
	test_signal.emit()
	test_signal_with_args.emit(42, "TestName")
	
	# Disconnect
	test_signal.disconnect(_on_test_signal)
	test_signal_with_args.disconnect(_on_test_signal_with_args)

func _on_test_signal():
	print("Signal received!")

func _on_test_signal_with_args(value: int, name: String):
	print("Signal with args: value=", value, ", name=", name)

func test_enums():
	print("\n--- Testing Enums ---")
	var color = Colors.RED
	print("color enum: ", color)
	print("color name: ", Colors.keys()[color])
	
	match color:
		Colors.RED:
			print("Color is RED")
		Colors.GREEN:
			print("Color is GREEN")
		Colors.BLUE:
			print("Color is BLUE")
	
	var flags = Flags.FLAG_A | Flags.FLAG_B
	print("flags: ", flags)
	print("has FLAG_A: ", flags & Flags.FLAG_A)

func test_type_checks():
	print("\n--- Testing Type Checks ---")
	var value = 42
	print("typeof(42): ", typeof(value))
	print("is int: ", value is int)
	print("is float: ", value is float)
	
	var node_ref = self
	print("is Node: ", node_ref is Node)
	print("is Object: ", node_ref is Object)

func test_lambdas():
	print("\n--- Testing Lambdas ---")
	var add_lambda = func(a, b): return a + b
	print("lambda add(2, 3): ", add_lambda.call(2, 3))
	
	var square = func(x): return x * x
	print("lambda square(5): ", square.call(5))
	
	# Using lambda with array operations
	var numbers = [1, 2, 3, 4, 5]
	var doubled = numbers.map(func(x): return x * 2)
	print("mapped (x*2): ", doubled)
	
	var evens = numbers.filter(func(x): return x % 2 == 0)
	print("filtered (evens): ", evens)

func test_static():
	print("\n--- Testing Static ---")
	static_counter += 1
	print("static counter: ", static_counter)
	print("static function: ", static_multiply(3, 4))

static func static_multiply(a: int, b: int) -> int:
	return a * b

func _process(delta: float):
	pass  # Override but don't use

func _exit_tree():
	print("Node exiting tree")
