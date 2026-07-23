--TEST--
class inheritance and parent call vector 016
--FILE--
<?php
class GeneratedBase016 {
    protected $value = 83;
    public function value() { return $this->value; }
}
class GeneratedChild016 extends GeneratedBase016 {
    public function value() { return parent::value() + 5; }
}
$object = new GeneratedChild016(); echo $object->value();
--EXPECT--
88
