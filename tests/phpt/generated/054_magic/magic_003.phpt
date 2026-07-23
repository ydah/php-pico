--TEST--
magic get set and string conversion vector 003
--FILE--
<?php
class GeneratedMagic003 {
    private $value = 35;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic003(); echo $box->missing, ':';
$box->dynamic = 38; echo $box;
--EXPECT--
missing:35:box=38
