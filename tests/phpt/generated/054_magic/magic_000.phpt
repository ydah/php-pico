--TEST--
magic get set and string conversion vector 000
--FILE--
<?php
class GeneratedMagic000 {
    private $value = 2;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic000(); echo $box->missing, ':';
$box->dynamic = 5; echo $box;
--EXPECT--
missing:2:box=5
