--TEST--
class inheritance and parent call vector 007
--FILE--
<?php
class GeneratedBase007 {
    protected $value = 38;
    public function value() { return $this->value; }
}
class GeneratedChild007 extends GeneratedBase007 {
    public function value() { return parent::value() + 2; }
}
$object = new GeneratedChild007(); echo $object->value();
--EXPECT--
40
