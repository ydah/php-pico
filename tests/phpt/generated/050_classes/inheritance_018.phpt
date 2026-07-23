--TEST--
class inheritance and parent call vector 018
--FILE--
<?php
class GeneratedBase018 {
    protected $value = 93;
    public function value() { return $this->value; }
}
class GeneratedChild018 extends GeneratedBase018 {
    public function value() { return parent::value() + 1; }
}
$object = new GeneratedChild018(); echo $object->value();
--EXPECT--
94
