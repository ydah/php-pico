--TEST--
class inheritance and parent call vector 013
--FILE--
<?php
class GeneratedBase013 {
    protected $value = 68;
    public function value() { return $this->value; }
}
class GeneratedChild013 extends GeneratedBase013 {
    public function value() { return parent::value() + 2; }
}
$object = new GeneratedChild013(); echo $object->value();
--EXPECT--
70
