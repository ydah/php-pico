--TEST--
class inheritance and parent call vector 002
--FILE--
<?php
class GeneratedBase002 {
    protected $value = 13;
    public function value() { return $this->value; }
}
class GeneratedChild002 extends GeneratedBase002 {
    public function value() { return parent::value() + 3; }
}
$object = new GeneratedChild002(); echo $object->value();
--EXPECT--
16
