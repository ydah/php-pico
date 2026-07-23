--TEST--
magic get set and string conversion vector 009
--FILE--
<?php
class GeneratedMagic009 {
    private $value = 101;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic009(); echo $box->missing, ':';
$box->dynamic = 104; echo $box;
--EXPECT--
missing:101:box=104
