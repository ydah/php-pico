--TEST--
class inheritance and parent call vector 014
--FILE--
<?php
class GeneratedBase014 {
    protected $value = 73;
    public function value() { return $this->value; }
}
class GeneratedChild014 extends GeneratedBase014 {
    public function value() { return parent::value() + 3; }
}
$object = new GeneratedChild014(); echo $object->value();
--EXPECT--
76
