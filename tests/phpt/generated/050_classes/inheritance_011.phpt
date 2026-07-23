--TEST--
class inheritance and parent call vector 011
--FILE--
<?php
class GeneratedBase011 {
    protected $value = 58;
    public function value() { return $this->value; }
}
class GeneratedChild011 extends GeneratedBase011 {
    public function value() { return parent::value() + 6; }
}
$object = new GeneratedChild011(); echo $object->value();
--EXPECT--
64
