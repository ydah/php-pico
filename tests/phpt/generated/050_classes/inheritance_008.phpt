--TEST--
class inheritance and parent call vector 008
--FILE--
<?php
class GeneratedBase008 {
    protected $value = 43;
    public function value() { return $this->value; }
}
class GeneratedChild008 extends GeneratedBase008 {
    public function value() { return parent::value() + 3; }
}
$object = new GeneratedChild008(); echo $object->value();
--EXPECT--
46
