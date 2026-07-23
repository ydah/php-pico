--TEST--
class inheritance and parent call vector 015
--FILE--
<?php
class GeneratedBase015 {
    protected $value = 78;
    public function value() { return $this->value; }
}
class GeneratedChild015 extends GeneratedBase015 {
    public function value() { return parent::value() + 4; }
}
$object = new GeneratedChild015(); echo $object->value();
--EXPECT--
82
