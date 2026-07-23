--TEST--
class inheritance and parent call vector 009
--FILE--
<?php
class GeneratedBase009 {
    protected $value = 48;
    public function value() { return $this->value; }
}
class GeneratedChild009 extends GeneratedBase009 {
    public function value() { return parent::value() + 4; }
}
$object = new GeneratedChild009(); echo $object->value();
--EXPECT--
52
