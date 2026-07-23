--TEST--
magic get set and string conversion vector 014
--FILE--
<?php
class GeneratedMagic014 {
    private $value = 156;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic014(); echo $box->missing, ':';
$box->dynamic = 159; echo $box;
--EXPECT--
missing:156:box=159
