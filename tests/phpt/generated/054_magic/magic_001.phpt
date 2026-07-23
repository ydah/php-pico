--TEST--
magic get set and string conversion vector 001
--FILE--
<?php
class GeneratedMagic001 {
    private $value = 13;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic001(); echo $box->missing, ':';
$box->dynamic = 16; echo $box;
--EXPECT--
missing:13:box=16
