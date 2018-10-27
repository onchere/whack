module Main

extern func puts(char*...);

func t2() bool {
	return true;
}

type Rest struct { char* msg; }

func (Rest) amazing(bool rr) int {
	_ = t2();
	if rr puts(this.msg);
	return 0;
}

func (Rest) ama(int b) int {
	t2();
	return b;
}

func testPartialApply(char* one, char* two) {
	puts(one);
	puts(two);
}

func main(int argc, char** argv) int {
	Rest r;
	r.msg = "Wello, Horld!";
	t2->r.amazing();
	r.msg = "Hello, World!";
	let ret = r.amazing->r.ama(true);
	match type(r.amazing) {
	  func(bool -> int): puts("Rest#amazing");
	}
	let apply = testPartialApply("Foo, ", ...);
	apply("Bar!");
	return ret;
}
