--TEST--
class method closure captures this
--FILE--
<?php
class Counter {
    private $value;
    public function __construct($value) { $this->value = $value; }
    public function callback() { return fn($step) => $this->value + $step; }
}
$callback = (new Counter(40))->callback();
echo $callback(2);
--EXPECT--
42
