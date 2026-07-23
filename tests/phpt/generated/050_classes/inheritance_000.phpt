--TEST--
class inheritance and parent call vector 000
--FILE--
<?php
class GeneratedBase000 {
    protected $value = 3;
    public function value() { return $this->value; }
}
class GeneratedChild000 extends GeneratedBase000 {
    public function value() { return parent::value() + 1; }
}
$object = new GeneratedChild000(); echo $object->value();
--EXPECT--
4
