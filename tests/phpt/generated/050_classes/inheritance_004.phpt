--TEST--
class inheritance and parent call vector 004
--FILE--
<?php
class GeneratedBase004 {
    protected $value = 23;
    public function value() { return $this->value; }
}
class GeneratedChild004 extends GeneratedBase004 {
    public function value() { return parent::value() + 5; }
}
$object = new GeneratedChild004(); echo $object->value();
--EXPECT--
28
