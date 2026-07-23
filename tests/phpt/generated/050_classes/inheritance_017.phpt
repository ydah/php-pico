--TEST--
class inheritance and parent call vector 017
--FILE--
<?php
class GeneratedBase017 {
    protected $value = 88;
    public function value() { return $this->value; }
}
class GeneratedChild017 extends GeneratedBase017 {
    public function value() { return parent::value() + 6; }
}
$object = new GeneratedChild017(); echo $object->value();
--EXPECT--
94
