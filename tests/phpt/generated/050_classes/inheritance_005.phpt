--TEST--
class inheritance and parent call vector 005
--FILE--
<?php
class GeneratedBase005 {
    protected $value = 28;
    public function value() { return $this->value; }
}
class GeneratedChild005 extends GeneratedBase005 {
    public function value() { return parent::value() + 6; }
}
$object = new GeneratedChild005(); echo $object->value();
--EXPECT--
34
