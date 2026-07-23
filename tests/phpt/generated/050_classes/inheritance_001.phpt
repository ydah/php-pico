--TEST--
class inheritance and parent call vector 001
--FILE--
<?php
class GeneratedBase001 {
    protected $value = 8;
    public function value() { return $this->value; }
}
class GeneratedChild001 extends GeneratedBase001 {
    public function value() { return parent::value() + 2; }
}
$object = new GeneratedChild001(); echo $object->value();
--EXPECT--
10
