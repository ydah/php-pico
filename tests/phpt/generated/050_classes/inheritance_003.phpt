--TEST--
class inheritance and parent call vector 003
--FILE--
<?php
class GeneratedBase003 {
    protected $value = 18;
    public function value() { return $this->value; }
}
class GeneratedChild003 extends GeneratedBase003 {
    public function value() { return parent::value() + 4; }
}
$object = new GeneratedChild003(); echo $object->value();
--EXPECT--
22
