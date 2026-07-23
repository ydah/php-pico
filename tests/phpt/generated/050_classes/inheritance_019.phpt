--TEST--
class inheritance and parent call vector 019
--FILE--
<?php
class GeneratedBase019 {
    protected $value = 98;
    public function value() { return $this->value; }
}
class GeneratedChild019 extends GeneratedBase019 {
    public function value() { return parent::value() + 2; }
}
$object = new GeneratedChild019(); echo $object->value();
--EXPECT--
100
