--TEST--
class inheritance and parent call vector 012
--FILE--
<?php
class GeneratedBase012 {
    protected $value = 63;
    public function value() { return $this->value; }
}
class GeneratedChild012 extends GeneratedBase012 {
    public function value() { return parent::value() + 1; }
}
$object = new GeneratedChild012(); echo $object->value();
--EXPECT--
64
