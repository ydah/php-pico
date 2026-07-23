--TEST--
class inheritance and parent call vector 006
--FILE--
<?php
class GeneratedBase006 {
    protected $value = 33;
    public function value() { return $this->value; }
}
class GeneratedChild006 extends GeneratedBase006 {
    public function value() { return parent::value() + 1; }
}
$object = new GeneratedChild006(); echo $object->value();
--EXPECT--
34
