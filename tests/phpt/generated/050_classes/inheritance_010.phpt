--TEST--
class inheritance and parent call vector 010
--FILE--
<?php
class GeneratedBase010 {
    protected $value = 53;
    public function value() { return $this->value; }
}
class GeneratedChild010 extends GeneratedBase010 {
    public function value() { return parent::value() + 5; }
}
$object = new GeneratedChild010(); echo $object->value();
--EXPECT--
58
